#version 420 core

// NOTE(stekap): Caution when ordering data in uniform buffer because of shader alignment
//               struct members and the whole struct itself.

#define BIAS               0.0001
#define MAX_FLOAT          3.40282347e+38f
#define MAX_SPHERE_COUNT   32
#define MAX_MATERIAL_COUNT 32
#define MAX_TRIANGLE_COUNT 32

struct Sphere {
	vec3 p;
	float r;
	unsigned int mat_index;
};

struct Triangle {
	vec3 p1;
	unsigned int mat_index;
	vec3 p2;
	vec3 p3;
};

// NOTE(stekap): Right hand rule.
vec3 triangle_normal(in Triangle t) {
	return normalize(cross(t.p2 - t.p1, t.p3 - t.p1));
}

struct Camera {
	vec3 p;
	vec3 x;
	vec3 y;
	vec3 z;

	float f;
};

struct Ray {
	vec3 p;
	// NOTE(stekap): We use zero vector for direction to denote the invalid vector.
	vec3 d;
	vec3 n;
	vec3 color;
	vec3 attenuation;
};

// Adjust when the ray struct changes.
#define RAY_SIZE_IN_VEC4 5

bool ray_invalid(Ray ray) {
	return ray.d == vec3(0.0, 0.0, 0.0);
}

void ray_invalidate(inout Ray ray) {
	ray.d = vec3(0.0, 0.0, 0.0);
}

struct Material {
	vec3 reflectance;
	float scatter;
	vec3 emittance;
	unsigned int flags;
};

in vec3 position;
layout (pixel_center_integer) in vec4 gl_FragCoord;

out vec4 fragment_color;

#define EXECUTION_TYPE_INITIALIZE        0
#define EXECUTION_TYPE_TRACE             1
#define EXECUTION_TYPE_INCLUDE_RAY_COLOR 2

uniform unsigned int execution_type;
// NOTE(stekap): This is here to allow us to properly scale image in real time during generation.
uniform unsigned int processed_ray_count;

uniform float time;
uniform float width;
uniform float height;

uniform unsigned int ray_count;
uniform unsigned int batch_jump_count;
uniform unsigned int sphere_count;
uniform unsigned int triangle_count;

uniform Camera camera;

// width * height * sizeof(Ray)
// Ray coordinates for the current fragment is defined by fragment coords.
// Every ray takes 5 float vec4 (for now).
// One dot '.' represents one float vec4.
// . . . . . | . . . . . | . . . . . |
// . . . . . | . . . . . | . . . . . |
// . . . . . | . . . . . | . . . . . |
// (0, 0) is at the bottom left.
layout (rgba32f, binding = 0) coherent uniform image2D batch_state;

Ray ray_load() {
	int X = RAY_SIZE_IN_VEC4 * int(gl_FragCoord.x);
	int Y =                    int(gl_FragCoord.y);
	return Ray(imageLoad(batch_state, ivec2(X+0, Y)).xyz,
			   imageLoad(batch_state, ivec2(X+1, Y)).xyz,
			   imageLoad(batch_state, ivec2(X+2, Y)).xyz,
			   imageLoad(batch_state, ivec2(X+3, Y)).xyz,
			   imageLoad(batch_state, ivec2(X+4, Y)).xyz);
}

void ray_store(Ray ray) {
	int X = RAY_SIZE_IN_VEC4 * int(gl_FragCoord.x);
	int Y =                    int(gl_FragCoord.y);
	imageStore(batch_state, ivec2(X+0, Y), vec4(ray.p,           0.0));
	imageStore(batch_state, ivec2(X+1, Y), vec4(ray.d,           0.0));
	imageStore(batch_state, ivec2(X+2, Y), vec4(ray.n,           0.0));
	imageStore(batch_state, ivec2(X+3, Y), vec4(ray.color,       1.0));
	imageStore(batch_state, ivec2(X+4, Y), vec4(ray.attenuation, 0.0));
}

layout (rgba32f, binding = 1) coherent uniform image2D final_colors;

vec4 color_load() {
	int X = int(gl_FragCoord.x);
	int Y = int(gl_FragCoord.y);
	return imageLoad(final_colors, ivec2(X, Y));
}

void color_store(vec4 color) {
	int X = int(gl_FragCoord.x);
	int Y = int(gl_FragCoord.y);
	imageStore(final_colors, ivec2(X, Y), color);
}

vec4 color_linear_to_sRGB(vec4 color) {
	// https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
	// https://entropymine.com/imageworsener/srgbformula/

	bvec3 cutoff = lessThan(color.rgb, vec3(0.0031308));
	vec3 higher = vec3(1.055)*pow(color.rgb, vec3(1.0/2.4)) - vec3(0.055);
	vec3 lower = color.rgb * vec3(12.92);
	return vec4(mix(higher, lower, cutoff), color.a);
}

// TODO(stekap): Light sources should somehow be grouped together when we start using direct light sampling.
//               Currently, it is not obvious what would be the efficient way for this, since in general,
//               these objects will have different type.

layout (std140, binding = 0) uniform Spheres {
	Sphere spheres[MAX_SPHERE_COUNT];
};

layout (std140, binding = 1) uniform Triangles {
	Triangle triangles[MAX_TRIANGLE_COUNT];
};

layout (std140, binding = 2) uniform Materials {
	Material materials[MAX_MATERIAL_COUNT];
};

// NOTE(stekap): LFSR_Rand_Gen is from: https://www.geeks3d.com/20100831/shader-library-noise-and-pseudo-random-number-generator-in-glsl/
//               The rest is custom made based on testing.

// TODO(stekap): Random number generator starts acting very weird and slows down after some number of iterations.
//               Not clear yet which part is the cause of this.

int LFSR_Rand_Gen(in int n)
{
  n = (n << 13) ^ n; 
  return (n * (n*n*15731+789221) + 1376312589) & 0x7fffffff;
}

float LFSR_Rand_Gen_f(in int n) {
	int x = LFSR_Rand_Gen(n);
	return float(sign(x)*x) / 2147483648;
}

float hash(vec3 xyz) {
	return fract(LFSR_Rand_Gen_f(int(dot(xyz, vec3(73719.123, 79.63401, 5527.8102)))));
}

// NOTE(stekap): Both hashes below seem decent. Distribution is geared towards the middle of the
//               [0, 1] range. First one seems to have worse distribution than the second but is a bit faster.
//               Constructed using ideas and tool from "https://thebookofshaders.com/10/".
//               First constructed is fract(tan(x)*1000.0).
//               Second constructed is smoothstep(0.0, 1.0, fract(tan(x)*1000.0)).
//               Then they are adjusted to depend on the 3d point.

// float hash(vec3 xyz) {
// 	return fract(tan(dot(xyz, vec3(12.9898, 78.233, 1.61803398)))*989.748192);
// }

// float hash(vec3 xyz) {
// 	return smoothstep(0.0, 1.0, fract(tan(dot(xyz, vec3(12.9898, 78.233, 1.61803398)))*1000.000));
// }

float random_0_to_1(vec3 xyz, float seed) {
	return hash(seed*xyz);
}

float random_minus_1_to_1(vec3 xyz, float seed) {
	return -1 + 2*hash(seed*xyz);
}

float random_in_range(vec3 xyz, float seed, float min, float max) {
	return min + (max - min)*hash(seed*xyz);
}

vec3 random_unit_vector(vec3 xyz, float seed) {
	return vec3(random_minus_1_to_1(xyz, seed + 0.1),
				random_minus_1_to_1(xyz, seed + 0.2),
				random_minus_1_to_1(xyz, seed + 0.3));
}

vec3 random_unit_vector_in_hemisphere(vec3 xyz, float seed, vec3 normal) {
	vec3 unit = random_unit_vector(xyz, seed);
	return sign(dot(unit, normal)) * unit;
}

void intersect_spheres(inout Ray ray, inout int sphere_index, inout float t) {
	// r = p + td
	// (A - c)*(A - c) = r^2
	// A*A - 2c*A + c*c = r^2

	// p*p + 2p*td + td*td - 2c*(p + td) + c*c = r^2
	// (t^2)(d*d) + (t)2(p*d - c*d) + p*p - 2c*p + c*c - r^2 = 0
	// (t^2)(d*d) + (t)2(p - c)*d + (p - c)*(p - c) - r^2 = 0

	for(int i = 0; i < sphere_count; ++i) {
		vec3 temp  = ray.p - spheres[i].p;
		float a    = dot(ray.d, ray.d);
		float b    = 2.0*dot(ray.d, temp);
		float D    = b*b - 4*a*(dot(temp, temp) - spheres[i].r*spheres[i].r);

		if(D > BIAS) {
			float sqrt_D = sqrt(D);
			// sqrt_D is > 0.
			// a is > 0.
			// If b is > 0, then t1 > t2.
			// If b is < 0, then t1 > t2.
			float t1 = 0.5 * (-b + sqrt_D) / a;
			float t2 = 0.5 * (-b - sqrt_D) / a;

			// ...t2...t1...|.............
			// ........t2...|...t1........
			// .............|...t2...t1...

			if(t2 > 0) {
				if(t2 < t) {
					t = t2;
					sphere_index = i;
				}
			}
			else if(t1 > 0) {
				if(t1 < t) {
					t = t1;
					sphere_index = i;
				}
			}
		}
		else if(D >= 0) {
			float te = 0.5 * (-b) / a;
			if(te > 0 && te < t) {
				t = te;
				sphere_index = i;
			}
		}
	}
}

// NOTE(stekap): This function assumes that the triangle points order matches the normal
//               via right hand rule.
void intersect_triangles(inout Ray ray, inout int triangle_index, inout float t) {
	// D   - ray direction
	// T   - ray origin minus triangle's first point
	// E1  - second triangle point minus first
	// E2  - third triangle point minus first
	// P   - D x E2
	// Q   - T x E1
	// det - P*E1

	// [t]     1   [Q*E2]
	// [u] = ----- [ P*T]
	// [v]    P*E1 [ Q*D]
	
	for(int i = 0; i < triangle_count; ++i) {
		vec3 E1 = triangles[i].p2 - triangles[i].p1;
		vec3 E2 = triangles[i].p3 - triangles[i].p1;

		vec3 P = cross(ray.d, E2);
		float det = dot(P, E1);

		// Hit on front side.
		if(det >= BIAS) {
			float inv_det = 1.0/det;
			vec3 T = ray.p - triangles[i].p1;

			float u = inv_det * dot(P, T);

			if(u >= 0 && u <= 1) {
				vec3 Q = cross(T, E1);
				float v = inv_det * dot(Q, ray.d);

				if(v >= 0 && v <= 1) {
					if(u + v <= 1) {
						float temp_t = inv_det * dot(Q, E2);
						if(temp_t > 0 && temp_t < t) {
							t = temp_t;
							triangle_index = i;
						}
					}
				}
			}
		}
		// Hit on back side.
		else if(det <= - BIAS) {

		}
	}
}

// TODO(stekap):
// For multiple lights, we choose to directly sample one, based on the probability assigned to that light.
// Alternatively, we could choose more than one and average the results.
// Probability is assigned based on the effect that the light can have on the given point (here, we can
// also take into account the photometric effect rather than radiometric).
// An example of such probability (not normalized) would be:
// light_area * light_power * cos(theta1) * cos(theta2) / r^2

// TODO(stekap): Direct light sampling is hardcoded for now. Change this after deciding on how the lights should be
//               organized in memory.
void direct_light_sample(inout Ray next_ray) {
	float t = MAX_FLOAT;
	int triangle_index = -1;
	
	Ray light_ray = next_ray;
	
	vec3 light_dir = vec3(278.0, 548.8, -275.0) - light_ray.p;
	
	light_ray.d = normalize(light_dir + 50*random_unit_vector_in_hemisphere(light_ray.p, time, vec3(0.0, 1.0, 0.0)));

	intersect_triangles(light_ray, triangle_index, t);
	
	if(triangle_index == 0 || triangle_index == 1) {
		float radiance_scaling = dot(light_ray.n, light_ray.d) * dot(-light_ray.d, triangle_normal(triangles[0])) / pow(distance(vec3(278.0, 548.8, -275.0), light_ray.p), 2);
		
		next_ray.color += next_ray.attenuation * materials[triangles[0].mat_index].emittance * radiance_scaling;
	}
}

void main() {
	float pixel_width = 2.0/width;
	float pixel_height = 2.0/height;
	float pixel_lower_radius = min(pixel_width, pixel_height);

	if(execution_type == EXECUTION_TYPE_INITIALIZE) {
		vec3 pixel_p = camera.p + position.x*camera.x + position.y*camera.y;
		vec3 focus   = camera.p + camera.f*camera.z;

		// NOTE(stekap): Something for generating random perturbation. Can be any vec3
		//               that is different for different samples of the same pixel.
		vec3 time_vec = vec3(time, time, time);

		vec3 pixel_p_perturbed = pixel_p + 0.5*random_in_range(time_vec, time + 0.1, -pixel_width, pixel_width)*camera.x + 0.5*random_in_range(time_vec, time + 0.2, -pixel_height, pixel_height)*camera.y;

		// Set the normal to be the same as direction for the initial ray i.e. ray comming from the camera focus.
		// This way, we know that the normal will not impact the calculation for the first hit, since there will
		// be no scaling with cosine term (since dot(direction, normal) = 1 in that case).
		vec3 ray_direction = normalize(pixel_p_perturbed - focus);
		Ray ray = {focus, ray_direction, ray_direction, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0)};
		
		ray_store(ray);

		return;
	}

	if(execution_type == EXECUTION_TYPE_TRACE) {
		// vec3 background_color = vec3(0.9, 0.9, 0.9);
		vec3 background_color = vec3(0.0, 0.0, 0.0);

		Ray ray = ray_load();

		if(ray_invalid(ray)) {
			return;
		}

		for(int jump_index = 0; jump_index < batch_jump_count; ++jump_index) {
			float t = MAX_FLOAT;
			int triangle_index = -1;
			int sphere_index = -1;

			intersect_triangles(ray, triangle_index, t);
			intersect_spheres(ray, sphere_index, t);

			if(triangle_index >= 0) {
				Triangle triangle = triangles[triangle_index];
				Material material = materials[triangle.mat_index];

				vec3 normal = triangle_normal(triangle);
				Ray next_ray;
				next_ray.p = ray.p + t*ray.d;

				// TODO(stekap): Adhoc. Revisit when dielectric handling is introduced.
				if(dot(ray.d, normal) < 0) {
					next_ray.p -= BIAS*normal;
				}
				else {
					next_ray.p += BIAS*normal;
				}
				
				next_ray.d = mix(reflect(ray.d, normal),
								 random_unit_vector_in_hemisphere(next_ray.p, time, normal),
								 material.scatter);

				next_ray.n = normal;
				next_ray.color = ray.color;
				next_ray.attenuation = ray.attenuation;

				float radiance_scaling = dot(ray.d, ray.n) * dot(-ray.d, normal) / pow(distance(next_ray.p, ray.p), 2);

				// Collect emittance.
				next_ray.color += ray.attenuation * material.emittance * radiance_scaling;

				// Collect attenuation.
				next_ray.attenuation *= material.reflectance;

				direct_light_sample(next_ray);

				ray = next_ray;
				
				// TODO(stekap): If it is emitter, then don't jump further. Will change.
				if(material.flags == 1) {
					ray_invalidate(ray);
					break;
				}
			}
			else if(sphere_index >= 0) {
				Sphere sphere     = spheres[sphere_index];
				Material material = materials[sphere.mat_index];
				
				ray.p = ray.p + t*ray.d;
				vec3 normal = normalize(ray.p - sphere.p);
				ray.d = reflect(ray.d, normal);

				ray.d = mix(ray.d, random_unit_vector_in_hemisphere(ray.p, time, normal), material.scatter);
				
				if(dot(ray.d, normal) < 0) {
					ray.p -= BIAS*normal;
				}
				else {
					ray.p += BIAS*normal;
				}

				// Collect emittance.
				ray.color += ray.attenuation * material.emittance;
				// Collect attenuation.
				ray.attenuation *= material.reflectance;
			}
			else {
				// Add because the sky behaves like emitter.
				ray.color += ray.attenuation * background_color;
				ray_invalidate(ray);

				break;
			}
		}

		ray_store(ray);

		return;
	}

	if(execution_type == EXECUTION_TYPE_INCLUDE_RAY_COLOR) {
		Ray ray = ray_load();

		// NOTE(stekap): We could maybe sample some light at the end of ray tracing for the current ray
		//               here, and add ray.attenuation*(sampled light) to the ray.color below.
		vec4 color = color_load() + vec4(ray.color/ray_count, 1.0);
		color_store(color);

		// NOTE(stekap): This allows us to properly show image generation in real time.
		fragment_color = vec4((color.xyz*ray_count)/processed_ray_count, 1);

		return;
	}
}

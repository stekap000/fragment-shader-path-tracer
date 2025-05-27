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
	vec3 color;
	vec3 attenuation;
};

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
// Every ray takes 4 float vec4.
// One dot '.' represents one float vec4.
// . . . . | . . . . | . . . . |
// . . . . | . . . . | . . . . |
// . . . . | . . . . | . . . . |
// (0, 0) is at the bottom left.
layout (rgba32f, binding = 0) coherent uniform image2D batch_state;

Ray ray_load() {
	int X = 4 * int(gl_FragCoord.x);
	int Y =     int(gl_FragCoord.y);
	return Ray(imageLoad(batch_state, ivec2(X+0, Y)).xyz,
			   imageLoad(batch_state, ivec2(X+1, Y)).xyz,
			   imageLoad(batch_state, ivec2(X+2, Y)).xyz,
			   imageLoad(batch_state, ivec2(X+3, Y)).xyz);
}

void ray_store(Ray ray) {
	int X = 4 * int(gl_FragCoord.x);
	int Y =     int(gl_FragCoord.y);
	imageStore(batch_state, ivec2(X+0, Y), vec4(ray.p,           0.0));
	imageStore(batch_state, ivec2(X+1, Y), vec4(ray.d,           0.0));
	imageStore(batch_state, ivec2(X+2, Y), vec4(ray.color,       1.0));
	imageStore(batch_state, ivec2(X+3, Y), vec4(ray.attenuation, 0.0));
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

// NOTE(stekap): Expects xy to be some larger range of values, for example [0, width] for x coordinate,
//               instead of just being a float from 0 to 1.
float gold_noise(vec2 xy, float seed){
	return fract(tan(distance(xy*1.61803398874989484820459, xy)*seed)*xy.x);
}

float hash1( vec2 a )
{
    return fract( sin( a.x * 3433.8 + a.y * 3843.98 ) * 45933.8 );
}

// NOTE(stekap): This seems to have speed like gold_noise and hash1. Better distribution than hash1.
//               Similar distribution like gold_noise. No problems like blue dots that gold_noise produces.
float hash2(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

// float noise(vec3 xyz, float seed){
// 	xyz *= vec3(width, height, 0);
// 	return fract(tan(distance(xyz*1.61803398874989484820459, xyz)*seed)*xyz.x);
// }

// float noise(vec3 xyz, float seed) {
// 	return hash1(seed*xyz.xy);
// }

// float noise(vec3 xyz, float seed) {
// 	return hash2(seed*xyz.xy);
// }

// float noise(vec3 xyz, float seed) {
// 	xyz *= vec3(width, height, 1);
// 	return gold_noise(xyz.xy, seed);
// }

float random_0_to_1(vec3 xyz, float seed) {
	return hash2(seed*xyz.xy);
}

float random_minus_1_to_1(vec3 xyz, float seed) {
	return -1 + 2*hash2(seed*xyz.xy);
}

float random_in_range(vec3 xyz, float seed, float min, float max) {
	return min + (max - min)*hash2(seed*xyz.xy);
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
		
		Ray ray = {focus, normalize(pixel_p_perturbed - focus), vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0)};
		
		ray_store(ray);

		return;
	}

	if(execution_type == EXECUTION_TYPE_TRACE) {
		vec3 background_color = vec3(0.9, 0.9, 0.9);

		Ray ray = ray_load();

		if(ray_invalid(ray)) {
			return;
		}

		for(int jump_index = 0; jump_index < batch_jump_count; ++jump_index) {
			float t = MAX_FLOAT;
			int sphere_index = -1;
			int triangle_index = -1;

			intersect_spheres(ray, sphere_index, t);
			intersect_triangles(ray, triangle_index, t);

			if(triangle_index >= 0) {
				ray.p = ray.p + t*ray.d;
				vec3 normal = triangle_normal(triangles[triangle_index]);
				ray.d = reflect(ray.d, normal);

				ray.d = mix(ray.d,
							random_unit_vector_in_hemisphere(ray.p, time, normal),
							materials[triangles[triangle_index].mat_index].scatter);

				if(dot(ray.d, normal) < 0) {
					ray.p -= BIAS*normal;
				}
				else {
					ray.p += BIAS*normal;
				}

				// TODO(stekap): Cosine term, inverse square law....

				// Collect emittance.
				ray.color += ray.attenuation * materials[triangles[triangle_index].mat_index].emittance;

				// Collect attenuation.
				ray.attenuation *= materials[triangles[triangle_index].mat_index].reflectance;
			}
			else if(sphere_index >= 0) {
				ray.p = ray.p + t*ray.d;
				vec3 normal = normalize(ray.p - spheres[sphere_index].p);
				ray.d = reflect(ray.d, normal);

				ray.d = mix(ray.d,
							random_unit_vector_in_hemisphere(ray.p, time, normal),
							materials[spheres[sphere_index].mat_index].scatter);
				
				if(dot(ray.d, normal) < 0) {
					ray.p -= BIAS*normal;
				}
				else {
					ray.p += BIAS*normal;
				}

				// TODO(stekap): Cosine term, inverse square law....

				// Collect emittance.
				ray.color += ray.attenuation * materials[spheres[sphere_index].mat_index].emittance;
				// Collect attenuation.
				ray.attenuation *= materials[spheres[sphere_index].mat_index].reflectance;
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

		vec4 color = color_load() + vec4((ray.attenuation * ray.color) / ray_count, 1.0);
		color_store(color);

		// NOTE(stekap): This allows us to properly show image generation in real time.
		fragment_color = vec4((color.xyz*ray_count)/processed_ray_count, 1);

		return;
	}
}

#version 420 core

// NOTE(stekap): Caution when ordering the data in the uniform buffer because of the shader alignment of
//               struct members and the whole struct itself.

#define BIAS               0.0001
#define MAX_FLOAT          3.40282347e+38f
#define PI                 3.14159265358979323846

#define MAX_SPHERE_COUNT   32
#define MAX_MATERIAL_COUNT 16384
#define MAX_TRIANGLE_COUNT 64
#define MAX_BVH_NODE_COUNT 65536
#define MAX_BVH_STACK_SIZE 128

#define USE_BVH true

struct Sphere {
	vec3 p;
	float r;
	unsigned int mat_index;
};

vec3 sphere_normal(in Sphere s, in vec3 collision_point) {
	return normalize(collision_point - s.p);
}

struct Triangle {
	vec3 p1;
	unsigned int mat_index;
	vec3 p2;
	vec3 p3;
};

// NOTE(stekap): Right hand rule.
// NOTE(stekap): Current assumption is that the normal is the same in every point of the triangle.
vec3 triangle_normal(in Triangle t) {
	return normalize(cross(t.p2 - t.p1, t.p3 - t.p1));
}

struct Material {
	vec3 albedo;
	float scatter_or_ior;
	vec3 emittance;
	unsigned int type;
};

#define MATERIAL_TYPE_NONE       0
#define MATERIAL_TYPE_BLACKBODY  1
#define MATERIAL_TYPE_DIFFUSE    2
#define MATERIAL_TYPE_SPECULAR   3
#define MATERIAL_TYPE_DIELECTRIC 4

struct PackedNode {
	// NOTE(stekap): We don't have explicit AABB type in shader that would contain 6 floats, because that would cause
	//               the GLSL to pad that structure to size 32. We don't want that, since that place will be filled
	//               with children_index and triangle_count.
	float min_x;
	float min_y;
	float min_z;
	float max_x;
	float max_y;
	float max_z;
	unsigned int children_index;
	unsigned int triangle_count;
};

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
	float ior;
	vec3 attenuation;
	unsigned int origin_material;
};

// Adjust when the ray struct changes.
#define RAY_SIZE_IN_VEC4 5

bool ray_invalid(Ray ray) {
	return ray.d == vec3(0.0, 0.0, 0.0);
}

void ray_invalidate(inout Ray ray) {
	ray.d = vec3(0.0, 0.0, 0.0);
}

in vec3 position;
layout (pixel_center_integer) in vec4 gl_FragCoord;

out vec4 fragment_color;

#define EXECUTION_TYPE_INITIALIZE        0
#define EXECUTION_TYPE_TRACE             1
#define EXECUTION_TYPE_INCLUDE_RAY_COLOR 2
#define EXECUTION_TYPE_CONVERT_TO_SRGB   3

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

uniform unsigned int triangle_light_count;
uniform unsigned int sphere_light_count;

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

	vec4 v0 = imageLoad(batch_state, ivec2(X+0, Y));
	vec4 v1 = imageLoad(batch_state, ivec2(X+1, Y));
	vec4 v2 = imageLoad(batch_state, ivec2(X+2, Y));
	vec4 v3 = imageLoad(batch_state, ivec2(X+3, Y));
	vec4 v4 = imageLoad(batch_state, ivec2(X+4, Y));
	
	return Ray(v0.xyz,
			   v1.xyz,
			   v2.xyz,
			   v3.xyz,
			   float(v3.w),
			   v4.xyz,
			   unsigned int(v4.w));
}

void ray_store(Ray ray) {
	int X = RAY_SIZE_IN_VEC4 * int(gl_FragCoord.x);
	int Y =                    int(gl_FragCoord.y);
	imageStore(batch_state, ivec2(X+0, Y), vec4(ray.p,           0.0));
	imageStore(batch_state, ivec2(X+1, Y), vec4(ray.d,           0.0));
	imageStore(batch_state, ivec2(X+2, Y), vec4(ray.n,           0.0));
	imageStore(batch_state, ivec2(X+3, Y), vec4(ray.color,       ray.ior));
	imageStore(batch_state, ivec2(X+4, Y), vec4(ray.attenuation, ray.origin_material));
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

layout (std140, binding = 3) uniform BVH {
	PackedNode bvh[MAX_BVH_NODE_COUNT];
};

// NOTE(stekap): LFSR_Rand_Gen is from: https://www.geeks3d.com/20100831/shader-library-noise-and-pseudo-random-number-generator-in-glsl/
//               The rest is custom made based on testing.

int LFSR_Rand_Gen(in int n)
{
	n = (n << 13) ^ n; 
	return (n * (n*n*15731+789221) + 1376312589) & 0x7fffffff;
}

float LFSR_Rand_Gen_f(in int n) {
	// int x = LFSR_Rand_Gen(n);
	// return float(sign(x)*x) / 2147483648;

	return 1.0 - float((n * (n * n * 15731 + 789221) +	1376312589) & 0x7fffffff) / 1073741824.0;
}

float hash(vec3 xyz) {
	int n = int(dot(xyz, vec3(40.0, 6400.0, 0.0)));
	// int n = int(xyz.x*40.0 + xyz.y*6400.0 + xyz.z*15000.0);
	return abs(LFSR_Rand_Gen_f(n));
}

// float hash(vec3 xyz) {
// 	return fract(LFSR_Rand_Gen_f(int(dot(xyz, vec3(9.812123, 79.63401, 5.8102)))));
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
	return normalize(vec3(random_minus_1_to_1(xyz, seed + 0.1),
						  random_minus_1_to_1(xyz, seed + 0.2),
						  random_minus_1_to_1(xyz, seed + 0.3)));
}

vec3 random_unit_vector_in_hemisphere(vec3 xyz, float seed, vec3 normal) {
	vec3 unit = random_unit_vector(xyz, seed);

	float d = dot(unit, normal);

	// NOTE(stekap): We need this check, since sign returns 0 if the argument is 0, which would return 0 vector.
	if(d == 0) return unit;
	
	return sign(d) * unit;
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

// NOTE(stekap): This procedure assumes that the objects are closed surfaces.
float intersect_triangle(in Ray ray, in Triangle triangle) {
	float t = MAX_FLOAT;

	vec3 E1 = triangle.p2 - triangle.p1;
	vec3 E2 = triangle.p3 - triangle.p1;

	vec3 P = cross(ray.d, E2);
	float det = dot(P, E1);

	// Hit on the front side of any triangle, or on the back side of the dielectric.
	if(det >= BIAS || (det <= -BIAS && materials[triangle.mat_index].type == MATERIAL_TYPE_DIELECTRIC)) {
		float inv_det = 1.0/det;
		vec3 T = ray.p - triangle.p1;

		float u = inv_det * dot(P, T);

		if(u >= 0 && u <= 1) {
			vec3 Q = cross(T, E1);
			float v = inv_det * dot(Q, ray.d);

			if(v >= 0 && v <= 1) {
				if(u + v <= 1) {
					float temp_t = inv_det * dot(Q, E2);
					if(temp_t > 0 && temp_t < t) {
						t = temp_t;
					}
				}
			}
		}
	}

	return t;
}

// NOTE(stekap): This function assumes that the triangle points order matches the normal
//               via right hand rule.
void intersect_triangles(in Ray ray, inout int triangle_index, inout float t) {
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

		// Hit on the front side of any triangle, or on the back side of the dielectric.
		if(det >= BIAS || (det <= -BIAS && materials[triangles[i].mat_index].type == MATERIAL_TYPE_DIELECTRIC)) {
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
	}
}

// TODO(stekap): Schlick's approximation if needed.
// TODO(stekap): Look again carefully at Jacobian refraction term scaling.
// TODO(stekap): Minimize branching after making sure that refraction calculations are correct.
void refract_ray(in Ray ray, inout Ray next_ray, in Material material, in vec3 normal) {
	float refraction_ratio = ray.ior / material.scatter_or_ior;
	bool inside = dot(ray.d, normal) > 0;

	if(inside) {
		refraction_ratio = ray.ior;
		normal = -normal;
	}

	float cos_i = -dot(ray.d, normal);
	float sin_t_sq = refraction_ratio*refraction_ratio*(1.0 - cos_i*cos_i);
	float under_root = 1 - sin_t_sq;

	float cos_t = sqrt(max(under_root, 0));
	vec3 refracted_dir = normalize(refraction_ratio*ray.d + (refraction_ratio*cos_i - cos_t)*normal);

	float R_p = pow((refraction_ratio*cos_i - cos_t)/(refraction_ratio*cos_i + cos_t), 2);
	float R_s = pow((cos_i - refraction_ratio*cos_t)/(cos_i + refraction_ratio*cos_t), 2);
	float reflection_probability = 0.5*(R_p + R_s);

	bool reflection = random_0_to_1(next_ray.p, time) < reflection_probability;

	if(inside) {
		normal = -normal;
	}

	if(inside) {
		// Total internal reflection.
		if(sin_t_sq > 1.0) {
			next_ray.p -= BIAS*normal;
			next_ray.d = reflect(ray.d, -normal);
			next_ray.ior = material.scatter_or_ior;
			next_ray.n = -normal;
		}
		else {
			if(reflection) {
				next_ray.p -= BIAS*normal;
				next_ray.d = reflect(ray.d, -normal);
				next_ray.ior = material.scatter_or_ior;
				next_ray.n = -normal;
			}
			else {
				next_ray.p += BIAS*normal;
				next_ray.d = refracted_dir;
				next_ray.ior = 1.0;
				next_ray.n = normal;
				next_ray.attenuation /= cos_t;
			}
		}
	}
	else {
		if(reflection) {
			next_ray.p += BIAS*normal;
			next_ray.d = reflect(ray.d, normal);
			next_ray.ior = 1.0;
			next_ray.n = normal;
		}
		else {
			next_ray.p -= BIAS*normal;
			next_ray.d = refracted_dir;
			next_ray.ior = material.scatter_or_ior;
			next_ray.n = -normal;
			next_ray.attenuation /= cos_t;
		}
	}
}

// Iterative form for the rendering equation:
// Lo1 = (1/n)SUM(material.emittance1 + (BRDF1/prob1)*cos(theta1)*Li1)
// Lo1 = (1/n)SUM(material.emittance1 + (BRDF1/prob1)*cos(theta1)*Lo2)
//     = (1/n)SUM(material.emittance1 + (BRDF1/prob1)*cos(theta1)*(material.emittance2 + (BRDF2/prob2)*cos(theta2)*Li2))
//
// <=> Lo1 = (1/n)SUM(material.emittance1*attenuation0 + attenuation1*(material.emittance2 + ...))

// attenuation0 = 1
// attenuation1 = (BRDF1/prob1)*cos(theta1)
// attenuation2 = (BRDF1/prob1)*cos(theta1) * (BRDF2/prob2)*cos(theta2)
// ...

void update_next_ray_diffuse_and_specular(in Ray ray, inout Ray next_ray, in Material material, in unsigned int mat_index, in vec3 normal, in float t) {
	next_ray.p = ray.p + t*ray.d;
	next_ray.n = normal;
	next_ray.ior = ray.ior;
	next_ray.color = ray.color;
	next_ray.attenuation = ray.attenuation;
	next_ray.origin_material = mat_index;

	if(dot(ray.d, normal) < 0) {
		next_ray.p += BIAS*normal;
	}
	else {
		next_ray.p -= BIAS*normal;
	}

	next_ray.d = normalize(mix(reflect(ray.d, normal),
							   random_unit_vector_in_hemisphere(next_ray.p, time, normal),
							   material.scatter_or_ior));
}

// TODO(stekap): For dielectric object, while we are inside of it, we don't need to waste time on iteration through all objects to figure
//               out which one we will hit, since we it must be that same object. Therefore, we can save some time by bouncing the ray inside,
//               until it exits the object.
void update_next_ray_dielectric(in Ray ray, inout Ray next_ray, in Material material, in unsigned int mat_index, vec3 normal, in float t) {
	next_ray.p = ray.p + t*ray.d;
	next_ray.color = ray.color;
	next_ray.attenuation = ray.attenuation;
	next_ray.origin_material = mat_index;

	refract_ray(ray, next_ray, material, normal);
}

// TODO(stekap): This is just temporary intersection. Later, it must be optimized with BVH.
void intersect_objects_flat(in Ray ray, inout int triangle_index, inout int sphere_index, inout float t) {
	intersect_triangles(ray, triangle_index, t);
	float t_temp = t;
	intersect_spheres(ray, sphere_index, t);
	if(t < t_temp) {
		triangle_index = -1;
	}
}

bool intersect_bvh_node(in Ray ray, in PackedNode node) {
	// TODO(stekap): Precalculate inverses to avoid division (after testing with the slower version, in order to gauge the improvement).
	// TODO(stekap): Minimize comparisons (min/max use), by taking into account that only 3 faces can be hit directly for the given ray direction.
	// TODO(stekap): NAN float value case not handled (arises when the ray origin is precisely on the boundary of the AABB). Maybe handle later.

	float t_close = -MAX_FLOAT;
	float t_far   =  MAX_FLOAT;

	float t1 = 0;
	float t2 = 0;

	t1 = (node.min_x - ray.p.x) / ray.d.x;
	t2 = (node.max_x - ray.p.x) / ray.d.x;

	t_close = max(t_close, min(t1, t2));
	t_far   = min(t_far,   max(t1, t2));

	t1 = (node.min_y - ray.p.y) / ray.d.y;
	t2 = (node.max_y - ray.p.y) / ray.d.y;

	t_close = max(t_close, min(t1, t2));
	t_far   = min(t_far,   max(t1, t2));

	t1 = (node.min_z - ray.p.z) / ray.d.z;
	t2 = (node.max_z - ray.p.z) / ray.d.z;

	t_close = max(t_close, min(t1, t2));
	t_far   = min(t_far,   max(t1, t2));

	// <= instead of < so that hit to the corner is included.
	return t_close <= t_far;
}

void intersect_objects_bvh(in Ray ray, inout int triangle_index, inout int sphere_index, inout float t) {
	unsigned int stack[MAX_BVH_STACK_SIZE];
	unsigned int top = 0;
	stack[top++] = 0;

	while(top != 0) {
		PackedNode node = bvh[stack[--top]];

		bool node_hit = intersect_bvh_node(ray, node);

		if(node_hit) {
			if(node.triangle_count > 0) {
				// NOTE(stekap): We only need to test for one triangle for now, since the leaves only have one (will probably change).
				float temp = intersect_triangle(ray, triangles[node.children_index]);
				if(temp < t) {
					t = temp;
					triangle_index = int(node.children_index);
				}
			}
			else {
				stack[top++] = node.children_index;
				stack[top++] = node.children_index + 1;
			}
		}
	}
}

void intersect_objects(in Ray ray, inout int triangle_index, inout int sphere_index, inout float t) {
	if(USE_BVH) {
		intersect_objects_bvh(ray, triangle_index, sphere_index, t);
	}
	else {
		intersect_objects_flat(ray, triangle_index, sphere_index, t);
	}
}

// TODO(stekap):
// For multiple lights, we choose to directly sample one, based on the probability assigned to that light.
// Alternatively, we could choose more than one and average the results.
// Probability is assigned based on the effect that the light can have on the given point (here, we can
// also take into account the photometric effect rather than radiometric).

// TODO(stekap): Direct light sampling is hardcoded for now. Change this after deciding on how the lights should be
//               organized in memory.

void direct_light_sample(inout Ray next_ray) {
	float t = MAX_FLOAT;
	int triangle_index = -1;
	int sphere_index = -1;
	
	Ray light_ray = next_ray;

	vec3 light_dir = vec3(278.0, 548.8, -275.0) - next_ray.p;

	// TODO(stekap): This ray is directed towards light i.e. it is a form of importance sampling. Think of this
	//               when multiple lights are included.
	light_ray.d = normalize(light_dir + 50*random_unit_vector_in_hemisphere(light_ray.p, time, vec3(0.0, 1.0, 0.0)));

	intersect_objects(light_ray, triangle_index, sphere_index, t);

	// Direct light sampling uses the area form of the integral in the rendering equation. This is why we don't just
	// have scaling with one cosine term, but with two plus with the inverse of distance squared.

	float visibility = float(triangle_index == 0 || triangle_index == 1);
	float geometry = dot(light_ray.n, light_ray.d) * dot(-light_ray.d, triangle_normal(triangles[0])) / pow(distance(vec3(278.0, 548.8, -275.0), light_ray.p), 2);
	float light_area = 13650;

	geometry = max(geometry, 0);

	next_ray.color += next_ray.attenuation * materials[triangles[0].mat_index].emittance * light_area * geometry * visibility;
}

void direct_light_sample_multiple_lights(inout Ray next_ray) {
	float t = MAX_FLOAT;
	int triangle_index = -1;
	int sphere_index = -1;

	unsigned int total_light_count = triangle_light_count + sphere_light_count;

	unsigned int light_index = int(floor(random_in_range(next_ray.p, time, 0, total_light_count)));

	// TODO(stekap): Make the uniform sampling version first, so that it can server as gauge.

	// data for every light:
	//   power calculation based on the material (should be precalculated for all light so that we can have a predefined PDF)
	//     these values can be stored as a prefix sum
	//     the way that the light material is used suggests that the emittance values are actually radiance,
	//     therefore, we could approximate the light's power by integrating L*projected_area*spatial_angle
	//   center and radius so that we can send a random ray within the cone
	//   light area (can be precalculated and stored)
}

bool update_ray(inout Ray ray, in Material material, in unsigned int mat_index, in vec3 normal, in float t) {
	if(material.type == MATERIAL_TYPE_BLACKBODY) {
		ray.color += ray.attenuation * dot(ray.d, ray.n) * material.emittance;
		return true;
	}

	Ray next_ray;
	float sampling_probability;
	vec3 BSDF;

	// TODO(stekap): Handling code has a similar form, so it could be subject to speedup.
	switch(material.type) {
		case MATERIAL_TYPE_DIFFUSE: {
			update_next_ray_diffuse_and_specular(ray, next_ray, material, mat_index, normal, t);

			// Actual BSDF is (albedo / PI).
			sampling_probability = 1.0 / (2*PI);
			BSDF = vec3(1.0 / PI);

			next_ray.attenuation *= material.albedo * (BSDF / sampling_probability) * dot(ray.d, ray.n);

			direct_light_sample(next_ray);
		} break;
		case MATERIAL_TYPE_SPECULAR: {
			update_next_ray_diffuse_and_specular(ray, next_ray, material, mat_index, normal, t);

			sampling_probability = 1.0;
			BSDF = vec3(1.0);

			next_ray.attenuation *= material.albedo * (BSDF / sampling_probability) * dot(ray.d, ray.n);
		} break;
		case MATERIAL_TYPE_DIELECTRIC: {
			update_next_ray_dielectric(ray, next_ray, material, mat_index, normal, t);

			// Actual sampling_probability is the 1 divided by the chosen Fresnel coefficient.
			// Actual BSDF is the chosen Fresnel coefficient.
			sampling_probability = 1.0;
			BSDF = vec3(1.0);

			next_ray.attenuation *= material.albedo * (BSDF / sampling_probability) * dot(ray.d, ray.n);
		} break;
	}

	ray = next_ray;

	return false;
}

// TODO(stekap): Decide on whether to use something like explicit material type or have properties fully encoded in parameters.
//               For example, when direct sampling is used we can't do it for specular surfaces since they obey Snell's law
//               and the direction is therefore fully determined. In that case, if we use explicit type, we would have
//                   if(material.type == MATERIAL_TYPE_SPECULAR) { direct_light_sample(...); }
//               However, if we don't have the explicit type and instead rely on some parameter like scatter, we would have
//                   direct_light_sample(...);
//               without 'if', and inside the body of that function, we would simply multiply calculated sample with scatter.

// TODO(stekap): Check if next_ray is even needed, or it is enough to just use ray.
void main() {
	float pixel_width = 2.0/width;
	float pixel_height = 2.0/height;
	float pixel_lower_radius = min(pixel_width, pixel_height);

	if(execution_type == EXECUTION_TYPE_INITIALIZE) {
		vec3 pixel_p = camera.p + position.x*camera.x + position.y*camera.y;
		vec3 focus   = camera.p + camera.f*camera.z;

		// NOTE(stekap): Something for generating random perturbation. Can be any vec3
		//               that is different for different samples of the same pixel.
		vec3 time_vec = vec3(time + 0.1, time + 0.2, time + 0.3);

		float perturbation_factor = 0.35;
		vec3 pixel_p_perturbed = pixel_p + perturbation_factor*random_in_range(time_vec, time + 0.1, -pixel_width, pixel_width)*camera.x + perturbation_factor*random_in_range(time_vec, time + 0.2, -pixel_height, pixel_height)*camera.y;

		// Set the normal to be the same as direction for the initial ray i.e. ray comming from the camera focus.
		// This way, we know that the normal will not impact the calculation for the first hit, since there will
		// be no scaling with cosine term (since dot(direction, normal) = 1 in that case).
		vec3 ray_direction = normalize(pixel_p_perturbed - focus);
		Ray ray = {focus, ray_direction, ray_direction, vec3(0.0, 0.0, 0.0), 1.0, vec3(1.0, 1.0, 1.0), 0};
		
		ray_store(ray);

		return;
	}

	if(execution_type == EXECUTION_TYPE_TRACE) {
		Ray ray = ray_load();

		if(ray_invalid(ray)) {
			return;
		}

		for(int jump_index = 0; jump_index < batch_jump_count; ++jump_index) {
			float t = MAX_FLOAT;
			int triangle_index = -1;
			int sphere_index = -1;

			intersect_objects(ray, triangle_index, sphere_index, t);

			if(triangle_index >= 0) {
				Triangle triangle = triangles[triangle_index];
				unsigned int mat_index = triangle.mat_index;
				Material material = materials[mat_index];
				vec3 normal = triangle_normal(triangle);

				bool light_hit = update_ray(ray, material, mat_index, normal, t);
				if(light_hit) {
					ray_invalidate(ray);
					break;
				}
			}
			else if(sphere_index >= 0) {
				Sphere sphere = spheres[sphere_index];
				unsigned int mat_index = sphere.mat_index;
				Material material = materials[sphere.mat_index];
				vec3 normal = sphere_normal(sphere, ray.p + t*ray.d);

				bool light_hit = update_ray(ray, material, mat_index, normal, t);
				if(light_hit) {
					ray_invalidate(ray);
					break;
				}
			}
			else {
				vec3 background_color = vec3(0.0, 0.0, 0.0);
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

		vec4 color = color_load() + vec4(ray.color/ray_count, 1.0);
		color_store(color);

		// NOTE(stekap): This allows us to properly show image generation in real time.
		fragment_color = vec4((color.xyz*ray_count)/processed_ray_count, 1);
		fragment_color = color_linear_to_sRGB(fragment_color);

		return;
	}

	if(execution_type == EXECUTION_TYPE_CONVERT_TO_SRGB) {
		color_store(color_linear_to_sRGB(color_load()));

		return;
	}
}

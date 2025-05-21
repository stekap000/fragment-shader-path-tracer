#version 420 core

// NOTE(stekap): Caution when ordering data in uniform buffer because of shader alignment
//               struct members and the whole struct itself.

#define BIAS               0.0001
#define MAX_FLOAT          3.40282347e+38f
#define MAX_SPHERE_COUNT   16
#define MAX_MATERIAL_COUNT 16
#define MAX_TRIANGLE_COUNT 16

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
	vec3 n;
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
	vec3 d;
};

struct Material {
	vec3 reflectance;
	float scatter;
	vec3 emittance;
};

in vec3 position;
out vec4 fragment_color;

uniform float time;

uniform float width;
uniform float height;
uniform unsigned int sphere_count;
uniform unsigned int triangle_count;

uniform Camera camera;

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

float noise(vec3 xyz, float seed) {
	return hash2(seed*xyz.xy);
}

// float noise(vec3 xyz, float seed) {
// 	xyz *= vec3(width, height, 1);
// 	return gold_noise(xyz.xy, seed);
// }

float rand_minus_one_to_one(vec3 xyz, float seed) {
	return -1 + 2*noise(xyz, seed);
}

float rand_in_range(vec3 xyz, float seed, float min, float max) {
	return min + (max - min)*noise(xyz, seed);
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
						if(temp_t < t) {
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
	
	vec3 background_color = vec3(0.4, 0.6, 0.8);
	vec3 color = vec3(0.0, 0.0, 0.0);
	vec3 attenuation = vec3(1.0, 1.0, 1.0);

	vec3 pixel_p = camera.p + position.x*camera.x + position.y*camera.y;
	vec3 focus   = camera.p + camera.f*camera.z;
	
	Ray original_ray = {focus, normalize(pixel_p - focus)};
	Ray ray = original_ray;
	float t = MAX_FLOAT;
	int sphere_index = -1;
	int triangle_index = -1;
	
	int ray_count = 8;
	int jump_count = 8;
	for(int ray_index = 0; ray_index < ray_count; ++ray_index) {
		color = vec3(0.0, 0.0, 0.0);
		attenuation = vec3(1.0, 1.0, 1.0);

		// Currently, random pixel position change is within range (-pixel_dimension, pixel_dimension).
		// If needed, we can experiment with lower range to make rays more focused.
		vec3 pixel_p_perturbed = pixel_p + 0.5*rand_in_range(ray.d, time + 0.1, -pixel_width, pixel_width)*camera.x + 0.5*rand_in_range(ray.d, time + 0.2, -pixel_height, pixel_height)*camera.y;

		ray.p = original_ray.p;
		ray.d = normalize(pixel_p_perturbed - focus);
		
		for(int jump_index = 0; jump_index < jump_count; ++jump_index) {
			t = MAX_FLOAT;
			sphere_index = -1;
			triangle_index = -1;

			intersect_spheres(ray, sphere_index, t);
			intersect_triangles(ray, triangle_index, t);

			if(triangle_index >= 0) {
				ray.p = ray.p + t*ray.d;
				vec3 normal = normalize(triangles[triangle_index].n);
				ray.d = reflect(ray.d, normal);

				float x = noise(ray.p, time + 0.1);
				float y = noise(ray.p, time + 0.2);
				float z = noise(ray.p, time + 0.3);
				ray.d = normalize(ray.d + materials[triangles[triangle_index].mat_index].scatter * normalize(vec3(x, y, z)));

				if(dot(ray.d, normal) < 0) {
					ray.p -= BIAS*normal;
				}
				else {
					ray.p += BIAS*normal;
				}

				// TODO(stekap): Cosine term.
				color += attenuation * materials[triangles[triangle_index].mat_index].emittance;
				attenuation *= materials[triangles[triangle_index].mat_index].reflectance;
			}
			else if(sphere_index >= 0) {
				ray.p = ray.p + t*ray.d;
				vec3 normal = normalize(ray.p - spheres[sphere_index].p);
				ray.d = reflect(ray.d, normal);

				float x = noise(ray.p, time + 0.1);
				float y = noise(ray.p, time + 0.2);
				float z = noise(ray.p, time + 0.3);
				ray.d = normalize(ray.d + materials[spheres[sphere_index].mat_index].scatter * normalize(vec3(x, y, z)));

				if(dot(ray.d, normal) < 0) {
					ray.p -= BIAS*normal;
				}
				else {
					ray.p += BIAS*normal;
				}

				// TODO(stekap): Cosine term.
				color += attenuation * materials[spheres[sphere_index].mat_index].emittance;
				attenuation *= materials[spheres[sphere_index].mat_index].reflectance;
			}
			else {
				// Add because the sky behaves like emitter.
				color += background_color;
				break;
			}
		}

		fragment_color += vec4(color * attenuation, 1.0);
	}

	fragment_color /= ray_count;

	// NOTE(stekap): For testing noise functions.
	// fragment_color = vec4(noise(vec3(position.x, position.y, 0), time + 0.1),
	// 					  noise(vec3(position.x, position.y, 0), time + 0.2),
	// 					  noise(vec3(position.x, position.y, 0), time + 0.3), 1.0);
}

#version 420 core

// NOTE(stekap): Caution when ordering data in uniform buffer because of shader alignment
//               struct members and the whole struct itself.

#define BIAS 0.0001
#define MAX_FLOAT 3.40282347e+38f
#define MAX_SPHERE_COUNT 16
#define MAX_MATERIAL_COUNT 16

struct Sphere {
	vec3 p;
	float r;
	unsigned int mat_index;
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

uniform Camera camera;

layout (std140, binding = 0) uniform Spheres {
	Sphere spheres[MAX_SPHERE_COUNT];
};

layout (std140, binding = 1) uniform Materials {
	Material materials[MAX_MATERIAL_COUNT];
};

// Gold Noise dcerisano@standard3d.com
// - based on the Golden Ratio
// - uniform normalized distribution
// - fastest static noise generator function (also runs at low precision)
// - use with indicated fractional seeding method. 

float PHI = 1.61803398874989484820459;

float gold_noise(vec2 xy, float seed){
	return fract(tan(distance(xy*PHI, xy)*seed)*xy.x);
}

void main() {
	vec3 background_color = vec3(0.2, 0.6, 0.8);
	vec3 color = vec3(0.0, 0.0, 0.0);
	vec3 attenuation = vec3(1.0, 1.0, 1.0);

	vec3 pixel_p = camera.p + position.x*camera.x + position.y*camera.y;
	vec3 focus   = camera.p + camera.f*camera.z;
	
	Ray ray = {focus, normalize(pixel_p - focus)};
	float t = MAX_FLOAT;
	int sphere_index = -1;

	// r = p + td
	// (A - c)*(A - c) = r^2
	// A*A - 2c*A + c*c = r^2

	// p*p + 2p*td + td*td - 2c*(p + td) + c*c = r^2
	// (t^2)(d*d) + (t)2(p*d - c*d) + p*p - 2c*p + c*c - r^2 = 0
	// (t^2)(d*d) + (t)2(p - c)*d + (p - c)*(p - c) - r^2 = 0

	for(int jump_count = 0; jump_count < 16; ++jump_count) {
		t = MAX_FLOAT;
		sphere_index = -1;
		
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

		if(sphere_index >= 0) {
			ray.p = ray.p + t*ray.d;
			vec3 sphere_normal = normalize(ray.p - spheres[sphere_index].p);
			ray.d = reflect(ray.d, sphere_normal);

			if(dot(ray.d, sphere_normal) < 0) {
				ray.p -= BIAS*sphere_normal;
			}
			else {
				ray.p += BIAS*sphere_normal;
			}

			attenuation *= materials[spheres[sphere_index].mat_index].reflectance;
		}
		else {
			// Add because the sky behaves like emitter.
			color += background_color;
			break;
		}
	}

	color *= attenuation;

	fragment_color = vec4(color, 1.0);
}

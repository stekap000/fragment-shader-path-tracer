#version 420 core

// NOTE(stekap): Caution when ordering data in uniform buffer because of shader alignment
//               struct members and the whole struct itself.

struct Sphere {
	vec3 p;
	float r;
	vec4 color;
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

in vec3 position;
out vec4 fragment_color;

uniform float time;

uniform float width;
uniform float height;
uniform unsigned int sphere_count;

uniform Camera camera;

#define bias 0.001
#define max_sphere_count 16

layout (std140, binding = 0) uniform Scene {
	Sphere spheres[max_sphere_count];
};

void main() {
	vec3 color = vec3(0.0, 0.0, 0.0);

	vec3 pixel_p = camera.p + position.x*camera.x + position.y*camera.y;
	vec3 focus   = camera.p + camera.f*camera.z;
	
	Ray ray = {focus, normalize(pixel_p - focus)};

	// r = p + td
	// (A - c)*(A - c) = r^2
	// A*A - 2c*A + c*c = r^2

	// p*p + 2p*td + td*td - 2c*(p + td) + c*c = r^2
	// (t^2)(d*d) + (t)2(p*d - c*d) + p*p - 2c*p + c*c - r^2 = 0
	// (t^2)(d*d) + (t)2(p - c)*d + (p - c)*(p - c) - r^2 = 0

	// t = (-b +- sqrt(b^2 - 4ac))/(2a)

	Sphere sphere = spheres[1];

	vec3 temp    = ray.p - sphere.p;
	float a      = dot(ray.d, ray.d);
	float b      = dot(2.0*ray.d, temp);
	float D      = b*b - 4*a*(dot(temp, temp) - sphere.r*sphere.r);
	// float sqrt_D = sqrt(D);

	if(D >= bias) {
		color += sphere.color.xyz;
	}

	fragment_color = vec4(color, 1.0);
	//fragment_color = vec4(sphere.color, 1.0);

	// fragment_color += vec4(0.0, 1 - pow(sin(15*position.x - 3*time) - 15*position.y, 2), 0.0, 1.0);
}

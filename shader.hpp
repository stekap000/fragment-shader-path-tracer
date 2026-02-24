#ifndef SHADER_HPP
#define SHADER_HPP

#include <iostream>

#include "shared.hpp"
#include "v3.hpp"

namespace BVH { struct AABB; }

// TODO(stekap): Maybe metaprogram part of the shader source, so that we can define things only once on the host
//               and have them in proper form in the shader (for example shared types and some constants like
//               max_triangle_count).

// NOTE(stekap): For types like Material that are shared between this code and shader code,
//               constructor parameters order represents more logical attribute order, but
//               the actual order differs from that in order to be more nicely packed for
//               shader.

// TODO(stekap): If needed, types that are shared with the shader should be packed better
//               (when their attributes and value ranges become more apparent).

enum : u32 {
	MATERIAL_TYPE_NONE       = 0,
	MATERIAL_TYPE_BLACKBODY  = 1,
	MATERIAL_TYPE_DIFFUSE    = 2,
	MATERIAL_TYPE_SPECULAR   = 3,
	MATERIAL_TYPE_DIELECTRIC = 4,
};

namespace ShaderConfig {
	Internal constexpr u32 max_sphere_count        = 32;
	Internal constexpr u32 max_triangle_count      = 16384;
	Internal constexpr u32 max_material_count      = 32;
	// This is around 2MB when using packed bvh nodes.
	Internal constexpr u32 max_bvh_node_count      = 65536;

	Internal constexpr u32 spheres_ub_bind_index   = 0;
	Internal constexpr u32 triangles_ub_bind_index = 1;
	Internal constexpr u32 materials_ub_bind_index = 2;
	Internal constexpr u32 bvh_ub_bind_index       = 3;

	Internal constexpr u32 batch_state_bind_index  = 0;
	Internal constexpr u32 final_colors_bind_index = 1;

	Internal constexpr u32 ray_size_in_vec4        = 5;
};

struct Material {
	V3 albedo;
	f32 scatter_or_ior;
	V3 emittance;
	// TODO(stekap): Currently, flags is more for testing. Maybe remove, maybe expand.
	u32 type;

	Material() {}
	Material(V3 albedo, V3 emittance, f32 scatter_or_ior, u32 type)
		: albedo(albedo), emittance(emittance), scatter_or_ior(scatter_or_ior), type(type) {}
};

struct Sphere {
	V3 p;
	f32 r;
	u32 mat_index;
	f32 SHADER_PAD[3];

	Sphere() {}
	Sphere(V3 p, f32 r, u32 mat_index) : p(p), r(r), mat_index(mat_index) {}
};

// TODO(stekap): Assume one normal per triangle for now. Later expand to per-vertex normals.
//               Also, texture coords should later be included.
// TODO(stekap): Probably store vertex array separately and have triangle struct hold indices
//               into such array.
struct Triangle {
	V3 p1;
	u32 mat_index;
	V3 p2;
	f32 SHADER_PAD0;
	V3 p3;
	f32 SHADER_PAD1;

	Triangle() {}
	Triangle(V3 p1, V3 p2, V3 p3, u32 mat_index)
		: p1(p1), p2(p2), p3(p3), mat_index(mat_index) {}

	inline void translate(const V3& translation_vector) {
		p1 += translation_vector;
		p2 += translation_vector;
		p3 += translation_vector;
	}

	inline void scale(const f32 f) {
		p1 *= f;
		p2 *= f;
		p3 *= f;
	}

	inline void rotate_y(const f32 deg) {
		#define PI 3.14159265358979323846

		p1.rotate_y(deg);
		p2.rotate_y(deg);
		p3.rotate_y(deg);

		#undef PI
	}

	V3 centroid() const {
		return (p1 + p2 + p3) / 3;
	}

	void print() const {
		std::cout << "T{[";
		std::cout << p1.x << " " << p1.y << " " << p1.z << "],";
		std::cout << p2.x << " " << p2.y << " " << p2.z << "],";
		std::cout << p3.x << " " << p3.y << " " << p3.z << "]}";
	}

	void println() const {
		print();
		std::cout << std::endl;
	}
};

struct Camera {
	V3 p, x, y, z;
	float f;

	Camera() {}
	Camera(V3 p, V3 x, V3 y, V3 z, float f) : p(p), x(x), y(y), z(z), f(f) {}

	static Camera cornell_box() {
		return Camera({278.0f, 274.0f, 600.0f},
					  {1.0f, 0.0f, 0.0f},
					  {0.0f, 1.0f, 0.0f},
					  {0.0f, 0.0f, 1.0f},
					  2.2f);
	}
};

#endif // SHADER_HPP

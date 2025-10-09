#ifndef SCENE_HPP
#define SCENE_HPP

#include <cassert>
#include <vector>

#include "shared.hpp"
#include "shader.hpp"
#include "io.hpp"
#include "bvh.hpp"

struct Scene {
	std::vector<Sphere> spheres;
	std::vector<Triangle> triangles;
	std::vector<Material> materials;

	std::vector<BVH::PackedNode> bvh;

	u32 triangle_light_count;
	u32 sphere_light_count;

	Scene() {}

	Scene(u32 sphere_count, u32 sphere_light_count, u32 triangle_count, u32 triangle_light_count, u32 material_count) {
		assert(sphere_count         <= ShaderConfig::max_sphere_count);
		assert(triangle_count       <= ShaderConfig::max_triangle_count);
		assert(material_count       <= ShaderConfig::max_material_count);
		assert(triangle_light_count <= ShaderConfig::max_triangle_count);
		assert(sphere_light_count   <= ShaderConfig::max_sphere_count);

		spheres.resize(sphere_count);
		triangles.resize(triangle_count);
		materials.resize(material_count);
	}

	Scene(std::vector<Sphere>& spheres, u32 sphere_light_count, std::vector<Triangle>& triangles, u32 triangle_light_count, std::vector<Material>& materials) {
		assert(spheres.size()       <= ShaderConfig::max_sphere_count);
		assert(triangles.size()     <= ShaderConfig::max_triangle_count);
		assert(materials.size()     <= ShaderConfig::max_material_count);
		assert(triangle_light_count <= ShaderConfig::max_triangle_count);
		assert(sphere_light_count   <= ShaderConfig::max_sphere_count);

		this->spheres = spheres;
		this->triangles = triangles;
		this->materials = materials;
		this->triangle_light_count = triangle_light_count;
		this->sphere_light_count = sphere_light_count;
	}

	void generate_bvh(int radius, int thread_count) {
		bvh = BVH::pack(BVH::construct(triangles, radius, thread_count));
	}

	static Scene cornell_box_with_lots_of_triangles() {
		std::vector<Sphere> spheres;
		std::vector<Triangle> triangles;
		std::vector<Material> materials;

		materials.push_back(Material({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f,   MATERIAL_TYPE_NONE));    // Default material
		materials.push_back(Material({0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE)); // White
		materials.push_back(Material({0.8f, 0.2f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE));    // Red
		materials.push_back(Material({0.2f, 0.8f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE));    // Green
		materials.push_back(Material({0.6f, 0.6f, 0.2f}, {5.0f, 5.0f, 2.5f}, 0.95f,  MATERIAL_TYPE_BLACKBODY));  // Light
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 0.005f, MATERIAL_TYPE_SPECULAR));   // Mirror
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.4f,   MATERIAL_TYPE_DIELECTRIC)); // Cube Glass
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.8f,   MATERIAL_TYPE_DIELECTRIC)); // Sphere Glass

		place_light(triangles, 4);

		place_back_wall(triangles, 1);
		place_left_wall(triangles, 2);
		place_right_wall(triangles, 3);
		place_floor(triangles, 1);
		place_ceiling(triangles, 1);

		std::vector<Triangle> model = IO::load_obj("models/hand.obj");

		for(Triangle& t : model) {
			t.rotate_y(45);
			t.scale(100);
			t.translate({250, 140, -200});
			t.mat_index = 1;
		}

		triangles.insert(triangles.end(), model.begin(), model.end());

		return Scene(spheres, 0, triangles, 2, materials);
	}

	static Scene cornell_box() {
		std::vector<Sphere> spheres;
		std::vector<Triangle> triangles;
		std::vector<Material> materials;

		materials.push_back(Material({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f,   MATERIAL_TYPE_NONE));    // Default material
		materials.push_back(Material({0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE)); // White

		materials.push_back(Material({0.8f, 0.2f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE));    // Red
		materials.push_back(Material({0.2f, 0.8f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE));    // Green

		// materials.push_back(Material({0.8f, 0.2f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.005f,  MATERIAL_TYPE_SPECULAR));
		// materials.push_back(Material({0.2f, 0.8f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.005f,  MATERIAL_TYPE_SPECULAR));

		materials.push_back(Material({0.6f, 0.6f, 0.2f}, {5.0f, 5.0f, 2.5f}, 0.95f,  MATERIAL_TYPE_BLACKBODY));  // Light
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 0.005f, MATERIAL_TYPE_SPECULAR));   // Mirror
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.4f,   MATERIAL_TYPE_DIELECTRIC)); // Cube Glass
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.8f,   MATERIAL_TYPE_DIELECTRIC)); // Sphere Glass

		place_light(triangles, 4);

		place_back_wall(triangles, 1);
		place_left_wall(triangles, 2);
		place_right_wall(triangles, 3);
		place_floor(triangles, 1);
		place_ceiling(triangles, 1);
		place_short_block(triangles, 1);
		place_tall_block(triangles, 1);

		// spheres.push_back(Sphere({200, 101, -200}, 100.0f, 7));
		// spheres.push_back(Sphere({430, 71, -100}, 70.0f, 7));

		return Scene(spheres, 0, triangles, 2, materials);
	}

	private : static void place_light(std::vector<Triangle>& triangles, int material_index) {
		// Light
		triangles.push_back(Triangle({343.0f, 548.799f, -227.0f}, {213.0f, 548.799f, -227.2f}, {343.0f, 548.799f, -332.0f}, material_index));
		triangles.push_back(Triangle({213.0f, 548.799f, -227.2f}, {213.0f, 548.799f, -332.0f}, {343.0f, 548.799f, -332.0f}, material_index));
	}

	private : static void place_back_wall(std::vector<Triangle>& triangles, int material_index) {
		// Back wall
		triangles.push_back(Triangle({0.0f, 0.0f, -559.2f}, {556.0f, 0.0f, -559.2f}, {556.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({556.0f, 548.8f, -559.2f}, {0.0f, 548.8f, -559.2f}, {0.0f, 0.0f, -559.2f}, material_index));
	}

	private : static void place_left_wall(std::vector<Triangle>& triangles, int material_index) {
		// Left wall
		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -559.2f}, {0.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({0.0f, 548.8f, -559.2f}, {0.0f, 548.8f, 0.0f}, {0.0f, 0.0f, 0.0f}, material_index));
	}

	private : static void place_right_wall(std::vector<Triangle>& triangles, int material_index) {
		// Right wall
		triangles.push_back(Triangle({556.0f, 0.0f, 0.0f}, {556.0f, 548.8f, 0.0f}, {556.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({556.0f, 548.8f, -559.2f}, {556.0f, 0.0f, -559.2f}, {556.0f, 0.0f, 0.0f}, material_index));
	}

	private : static void place_floor(std::vector<Triangle>& triangles, int material_index) {
		// Floor
		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {556.0f, 0.0f, 0.0f}, {556.0f, 0.0f, -559.2f}, material_index));
		triangles.push_back(Triangle({556.0f, 0.0f, -559.2f}, {0.0f, 0.0f, -559.2f}, {0.0f, 0.0f, 0.0f}, material_index));
	}

	private : static void place_ceiling(std::vector<Triangle>& triangles, int material_index) {
		// Ceiling
		triangles.push_back(Triangle({556.0f, 548.8f, 0.0f}, {0.0f, 548.8f, 0.0f}, {556.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({0.0f, 548.8f, -559.2f}, {556.0f, 548.8f, -559.2f}, {0.0f, 548.8f, 0.0f}, material_index));
	}

	private : static void place_short_block(std::vector<Triangle>& triangles, int material_index, V3 translation_vector = {0, 0, 0}) {
		size_t old_triangle_count = triangles.size();

		// Short block
		// Top
		triangles.push_back(Triangle({426.0f, 165.0f, -65.0f}, {474.0f, 165.0f, -225.0f}, {316.0f, 165.0f, -272.0f}, material_index));
		triangles.push_back(Triangle({266.0f, 165.0f, -114.0f}, {426.0f, 165.0f, -65.0f}, {316.0f, 165.0f, -272.0f}, material_index));

		// Left
		triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {266.0f, 165.0f, -114.0f}, {316.0f, 165.0f, -272.0f}, material_index));
		triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {316.0f, 165.0f, -272.0f}, {316.0f, 0.0f, -272.0f}, material_index));

		// Right
		triangles.push_back(Triangle({474.0f, 0.0f, -225.0f}, {474.0f, 165.0f, -225.0f}, {426.0f, 165.0f, -65.0f}, material_index));
		triangles.push_back(Triangle({474.0f, 0.0f, -225.0f}, {426.0f, 165.0f, -65.0f}, {426.0f, 0.0f, -65.0f}, material_index));

		// Front
		triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {426.0f, 165.0f, -65.0f}, {266.0f, 165.0f, -114.0f}, material_index));
		triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {266.0f, 165.0f, -114.0f}, {266.0f, 0.0f, -114.0f}, material_index));

		// Back
		triangles.push_back(Triangle({316.0f, 0.0f, -272.0f}, {316.0f, 165.0f, -272.0f}, {474.0f, 165.0f, -225.0f}, material_index));
		triangles.push_back(Triangle({316.0f, 0.0f, -272.0f}, {474.0f, 165.0f, -225.0f}, {474.0f, 0.0f, -225.0f}, material_index));

		// Bottom
		// triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {316.0f, 0.0f, -272.0f}, {474.0f, 0.0f, -225.0f}, material_index));
		// triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {316.0f, 0.0f, -272.0f}, {426.0f, 0.0f, -65.0f}, material_index));

		for(size_t i = old_triangle_count; i < old_triangle_count + (triangles.size() - old_triangle_count); ++i) {
			triangles[i].translate(translation_vector);
		}
	}

	private : static void place_tall_block(std::vector<Triangle>& triangles, int material_index, V3 translation_vector = {0, 0, 0}) {
		size_t old_triangle_count = triangles.size();

		// Tall block
		// Top
		triangles.push_back(Triangle({133.0f, 330.0f, -247.0f}, {291.0f, 330.0f, -296.0f}, {242.0f, 330.0f, -456.0f}, material_index));
		triangles.push_back(Triangle({133.0f, 330.0f, -247.0f}, {242.0f, 330.0f, -456.0f}, {84.0f, 330.0f, -406.0f}, material_index));

		// Left
		triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {133.0f, 330.0f, -247.0f}, {84.0f, 330.0f, -406.0f}, material_index));
		triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {84.0f, 330.0f, -406.0f}, {84.0f, 0.0f, -406.0f}, material_index));

		// Right
		triangles.push_back(Triangle({242.0f, 0.0f, -456.0f}, {242.0f, 330.0f, -456.0f}, {291.0f, 330.0f, -296.0f}, material_index));
		triangles.push_back(Triangle({242.0f, 0.0f, -456.0f}, {291.0f, 330.0f, -296.0f}, {291.0f, 0.0f, -296.0f}, material_index));

		// Front
		triangles.push_back(Triangle({291.0f, 0.0f, -296.0f}, {291.0f, 330.0f, -296.0f}, {133.0f, 330.0f, -247.0f}, material_index));
		triangles.push_back(Triangle({291.0f, 0.0f, -296.0f}, {133.0f, 330.0f, -247.0f}, {133.0f, 0.0f, -247.0f}, material_index));

		// Back
		triangles.push_back(Triangle({84.0f, 0.0f, -406.0f}, {84.0f, 330.0f, -406.0f}, {242.0f, 330.0f, -456.0f}, material_index));
		triangles.push_back(Triangle({84.0f, 0.0f, -406.0f}, {242.0f, 330.0f, -456.0f}, {242.0f, 0.0f, -456.0f}, material_index));

		// Bottom
		// triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {242.0f, 0.0f, -456.0f}, {291.0f, 0.0f, -296.0f}, material_index));
		// triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {84.0f, 0.0f, -406.0f}, {242.0f, 0.0f, -456.0f}, material_index));

		for(size_t i = old_triangle_count; i < old_triangle_count + (triangles.size() - old_triangle_count); ++i) {
			triangles[i].translate(translation_vector);
		}
	}
};

#endif // SCENE_HPP

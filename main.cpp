#include "glad/glad.h"
#include "glfw/glfw3.h"

#include <iostream>

#include "shared.hpp"
#include "window.hpp"
#include "shader.hpp"
#include "opengl.hpp"
#include "scene.hpp"
#include "tracer.hpp"
#include "bvh.hpp"

// NOTE(stekap): We don't bother with resources cleanup like allocated textures or whatever
//               since the program just generates the image and exits so the OS will do the cleanup.

// TODO(stekap): Revisit BVH construction and modify min and max to be robust (based on T. Ize).
//               Also, see if there is a need to add some padding for the AABB.
int main() {
	Window::create(400, 400);

	u32 ray_count        = 256;
	u32 ray_jump_count   = 32;
	u32 batch_jump_count = 32;

	// Scene scene = Scene::cornell_box();
	Scene scene = Scene::cornell_box_with_lots_of_triangles();
	scene.generate_bvh(5, std::thread::hardware_concurrency());

	Camera camera = Camera::cornell_box();
	u32 program = OpenGL::create_shader_program("shaders/batch.vert", "shaders/batch.frag");

	Tracer tracer(scene, camera, program);
	tracer.run(ray_count, ray_jump_count, batch_jump_count);

	return 0;
}

#include "glad/glad.h"
#include "glfw/glfw3.h"

// NOTE(stekap): We won't bother with resource cleanup like allocated textures or whatever
//               since the program just generates image and exits so the OS will do the cleanup.

#include <iostream>

#include "shared.hpp"
#include "window.hpp"
#include "shader.hpp"
#include "opengl.hpp"
#include "scene.hpp"
#include "tracer.hpp"
#include "bvh.hpp"

// TODO(stekap): Check if next_ray is even needed, or it is enough to just use ray.
int main(int arg_count, char** args) {
	// BVH::construction_test(); return 0;

	GLFWwindow* window = Window::create(400, 400, true);

	if(!window) {
		return -1;
	}

	OpenGL::initialize_tracer_rectangle();

	Scene scene   = Scene::cornell_box();
	Camera camera = Camera::cornell_box();
	
	u32 ray_count        = 512;
	u32 ray_jump_count   = 32;
	u32 batch_jump_count = 32;

	u32 program = OpenGL::create_shader_program("shaders/batch.vert", "shaders/batch.frag");

	Tracer tracer(window, scene, camera, program);

	tracer.run(ray_count, ray_jump_count, batch_jump_count);

	glfwTerminate();
	return 0;
}

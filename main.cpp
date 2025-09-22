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

// TODO(stekap): Check if next_ray is even needed, or it is enough to just use ray.
int main() {
	Window::create(400, 400);

	u32 ray_count        = 512;
	u32 ray_jump_count   = 32;
	u32 batch_jump_count = 32;

	Tracer tracer(Scene::cornell_box(),
				  Camera::cornell_box(),
				  OpenGL::create_shader_program("shaders/batch.vert", "shaders/batch.frag"));

	tracer.run(ray_count, ray_jump_count, batch_jump_count);

	return 0;
}

#ifndef TRACER_HPP
#define TRACER_HPP

#include "glad/glad.h"
#include "glfw/glfw3.h"

#include <chrono>
#include <thread>

#include "shared.hpp"
#include "shader.hpp"
#include "opengl.hpp"
#include "log.hpp"
#include "time.hpp"
#include "scene.hpp"

struct Tracer {
	GLFWwindow* window;
	Scene scene;
	Camera camera;
	u32 program;

	u32 spheres_ub;
	u32 triangles_ub;
	u32 materials_ub;

	enum : u32 {
		EXECUTION_TYPE_INITIALIZE         = 0,
		EXECUTION_TYPE_TRACE              = 1,
		EXECUTION_TYPE_INCLUDE_RAY_COLOR  = 2,
		EXECUTION_TYPE_CONVERT_TO_SRGB    = 3
	};

	Tracer() {}
	Tracer(GLFWwindow* window, Scene& scene, Camera& camera, u32 program) : window(window), scene(scene), camera(camera), program(program) {
		OpenGL::use_shader_program(program);
		initialize_scene();
		initialize_camera();
		initialize_program();
	}

	void initialize_scene() {
		spheres_ub   = OpenGL::create_uniform_buffer(ShaderConfig::max_sphere_count   * sizeof(Sphere));
		triangles_ub = OpenGL::create_uniform_buffer(ShaderConfig::max_triangle_count * sizeof(Triangle));
		materials_ub = OpenGL::create_uniform_buffer(ShaderConfig::max_material_count * sizeof(Material));

		glBindBufferRange(GL_UNIFORM_BUFFER, ShaderConfig::spheres_ub_bind_index,   spheres_ub,   0, ShaderConfig::max_sphere_count*sizeof(Sphere));
		glBindBufferRange(GL_UNIFORM_BUFFER, ShaderConfig::triangles_ub_bind_index, triangles_ub, 0, ShaderConfig::max_triangle_count*sizeof(Triangle));
		glBindBufferRange(GL_UNIFORM_BUFFER, ShaderConfig::materials_ub_bind_index, materials_ub, 0, ShaderConfig::max_material_count*sizeof(Material));

		glBindBuffer(GL_UNIFORM_BUFFER, spheres_ub);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, scene.spheres.size()*sizeof(Sphere), scene.spheres.data());
		glBindBuffer(GL_UNIFORM_BUFFER, triangles_ub);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, scene.triangles.size()*sizeof(Triangle), scene.triangles.data());
		glBindBuffer(GL_UNIFORM_BUFFER, materials_ub);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, scene.materials.size()*sizeof(Material), scene.materials.data());
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void initialize_camera() {
		glUniform3fv(glGetUniformLocation(program, "camera.p"), 1, (f32*)&camera.p);
		glUniform3fv(glGetUniformLocation(program, "camera.x"), 1, (f32*)&camera.x);
		glUniform3fv(glGetUniformLocation(program, "camera.y"), 1, (f32*)&camera.y);
		glUniform3fv(glGetUniformLocation(program, "camera.z"), 1, (f32*)&camera.z);
		glUniform1f(glGetUniformLocation(program, "camera.f"), camera.f);
	}

	void initialize_program() {
		glUniform1f(glGetUniformLocation(program, "width"), (f32)Window::width);
		glUniform1f(glGetUniformLocation(program, "height"), (f32)Window::height);
		glUniform1ui(glGetUniformLocation(program, "sphere_count"), (u32)scene.spheres.size());
		glUniform1ui(glGetUniformLocation(program, "triangle_count"), (u32)scene.triangles.size());
		glUniform1ui(glGetUniformLocation(program, "triangle_light_count"), (u32)scene.triangle_light_count);
		glUniform1ui(glGetUniformLocation(program, "sphere_light_count"), (u32)scene.sphere_light_count);
	}

	Internal void dispatch_batch(s32 execution_type_uniform_location, u32 execution_type) {
		glUniform1ui(execution_type_uniform_location, execution_type);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		// TODO(stekap): Think about this barrier and glFinish further.
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glFinish();
	}

	void run(u32 ray_count, u32 ray_jump_count, u32 batch_jump_count, bool debug = false) {
		u32 batch_count = (ray_jump_count / batch_jump_count);

		if(!debug) Log::batching_configuration(ray_count, batch_count, ray_jump_count, batch_jump_count);

		s32 execution_type_uniform_location = glGetUniformLocation(program, "execution_type");
		s32 time_uniform_location           = glGetUniformLocation(program, "time");

		glUniform1ui(glGetUniformLocation(program, "ray_count"), ray_count);
		glUniform1ui(glGetUniformLocation(program, "batch_jump_count"), batch_jump_count);

		u32 batch_state_texture  = OpenGL::create_and_bind_rgba32f_image2d(ShaderConfig::ray_size_in_vec4*Window::width, Window::height, ShaderConfig::batch_state_bind_index);
		__ignore__(batch_state_texture);
		u32 final_colors_texture = OpenGL::create_and_bind_rgba32f_image2d(Window::width, Window::height, ShaderConfig::final_colors_bind_index);
		__ignore__(final_colors_texture);

		u32 jumps_done = 0;
		double time_start = Time::now();
		double ray_time_start;
		double total_ray_time = 0;
		double percent_done = 0;
		double estimated_wait = 0;
		for(u32 ray_index = 0; ray_index < ray_count; ++ray_index) {
			ray_time_start = Time::now();

			glUniform1ui(glGetUniformLocation(program, "processed_ray_count"), ray_index + 1);

			dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_INITIALIZE);

			for(u32 batch_index = 0; batch_index < batch_count; ++batch_index) {
				glUniform1f(time_uniform_location, (f32)Time::now());
				dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_TRACE);
				jumps_done += batch_jump_count;
			}

			dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_INCLUDE_RAY_COLOR);

			total_ray_time += Time::now() - ray_time_start;

			if(!debug) {
				glfwSwapBuffers(window);
				percent_done   = (f64)jumps_done * 100 / (f64)(ray_count*ray_jump_count);
				estimated_wait = (total_ray_time / (ray_index + 1)) * (ray_count - ray_index - 1);

				Log::percent_done_and_estimated_wait(percent_done, estimated_wait);
			}
		}

		double total_time = Time::now() - time_start;

		if(!debug) {
			std::cout << std::endl;
			Log::measured_timings(total_time, ray_count, batch_count);

			dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_CONVERT_TO_SRGB);
			IO::save_final_output("generated_image.png");
		}
	}

	void debug(u32 ray_count, u32 ray_jump_count, u32 batch_jump_count) {
		s32 time_uniform_location   = glGetUniformLocation(program, "time");
		s32 width_uniform_location  = glGetUniformLocation(program, "width");
		s32 height_uniform_location = glGetUniformLocation(program, "height");

		double time_start;
		double time_end;

		time_start = Time::now();
		while(!glfwWindowShouldClose(window))
		{
			if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
				glfwSetWindowShouldClose(window, true);
			}

			if(glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
				std::this_thread::sleep_for(std::chrono::milliseconds(3000));
			}

			glUniform1f(time_uniform_location, (f32)Time::now());
			glUniform1f(width_uniform_location, (f32)Window::width);
			glUniform1f(height_uniform_location, (f32)Window::height);

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			run(ray_count, ray_jump_count, batch_jump_count, true);
			glfwSwapBuffers(window);

			glfwPollEvents();

			time_end = Time::now();
			glfwSetWindowTitle(window, std::to_string(time_end - time_start).c_str());
			time_start = time_end;
		}
	}
};

#endif // TRACER_HPP

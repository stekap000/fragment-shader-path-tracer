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
#include "window.hpp"
#include "bvh.hpp"

struct Tracer {
	Scene scene;
	Camera camera;
	u32 program;

	u32 spheres_ub;
	u32 triangles_ub;
	u32 materials_ub;
	u32 bvh_ub;

	enum : u32 {
		EXECUTION_TYPE_INITIALIZE         = 0,
		EXECUTION_TYPE_TRACE              = 1,
		EXECUTION_TYPE_INCLUDE_RAY_COLOR  = 2,
		EXECUTION_TYPE_CONVERT_TO_SRGB    = 3
	};

	Tracer() {}
	Tracer(const Scene& scene, const Camera& camera, const u32 program) : scene(scene), camera(camera), program(program) {
		OpenGL::initialize_tracer_rectangle();
		OpenGL::use_shader_program(program);
		initialize_scene();
		initialize_camera();
		initialize_program();
	}

	void initialize_scene() {
		spheres_ub   = OpenGL::create_uniform_buffer(ShaderConfig::max_sphere_count   * sizeof(Sphere));
		triangles_ub = OpenGL::create_uniform_buffer(ShaderConfig::max_triangle_count * sizeof(Triangle));
		materials_ub = OpenGL::create_uniform_buffer(ShaderConfig::max_material_count * sizeof(Material));
		bvh_ub       = OpenGL::create_uniform_buffer(ShaderConfig::max_bvh_node_count * sizeof(BVH::PackedNode));

		OpenGL::link_uniform_buffer_to_index(ShaderConfig::spheres_ub_bind_index,   spheres_ub,   ShaderConfig::max_sphere_count   * sizeof(Sphere));
		OpenGL::link_uniform_buffer_to_index(ShaderConfig::triangles_ub_bind_index, triangles_ub, ShaderConfig::max_triangle_count * sizeof(Triangle));
		OpenGL::link_uniform_buffer_to_index(ShaderConfig::materials_ub_bind_index, materials_ub, ShaderConfig::max_material_count * sizeof(Material));
		OpenGL::link_uniform_buffer_to_index(ShaderConfig::bvh_ub_bind_index,       bvh_ub,       ShaderConfig::max_bvh_node_count * sizeof(BVH::PackedNode));

		OpenGL::fill_uniform_buffer(spheres_ub,   scene.spheres.size()   * sizeof(Sphere),          scene.spheres.data());
		OpenGL::fill_uniform_buffer(triangles_ub, scene.triangles.size() * sizeof(Triangle),        scene.triangles.data());
		OpenGL::fill_uniform_buffer(materials_ub, scene.materials.size() * sizeof(Material),        scene.materials.data());
		OpenGL::fill_uniform_buffer(bvh_ub,       scene.bvh.size()       * sizeof(BVH::PackedNode), scene.bvh.data());
	}

	void initialize_camera() {
		OpenGL::uniform_f32_v3(program, "camera.p", camera.p);
		OpenGL::uniform_f32_v3(program, "camera.x", camera.x);
		OpenGL::uniform_f32_v3(program, "camera.y", camera.y);
		OpenGL::uniform_f32_v3(program, "camera.z", camera.z);
		OpenGL::uniform_f32_x1(program, "camera.f", camera.f);
	}

	void initialize_program() {
		OpenGL::uniform_f32_x1(program, "width",                (f32)Window::width);
		OpenGL::uniform_f32_x1(program, "height",               (f32)Window::height);
		OpenGL::uniform_u32_x1(program, "sphere_count",         (u32)scene.spheres.size());
		OpenGL::uniform_u32_x1(program, "sphere_light_count",   (u32)scene.sphere_light_count);
		OpenGL::uniform_u32_x1(program, "triangle_count",       (u32)scene.triangles.size());
		OpenGL::uniform_u32_x1(program, "triangle_light_count", (u32)scene.triangle_light_count);

		// NOTE(stekap): Right now, there is no need for material_count uniform in the shader, since we don't iterate on materials.
		//               Also, there is no need for bvh node count for the same reason.
		//               Additionally, counts for primitives (like triangle_count) are not needed if we use bvh, but we keep them for now.
	}

	void run(u32 ray_count, u32 ray_jump_count, u32 batch_jump_count, bool debug = false) {
		u32 batch_count = (ray_jump_count / batch_jump_count);

		if(!debug) {
			Log::scene_data(scene);
			Log::batching_configuration(ray_count, batch_count, ray_jump_count, batch_jump_count);
		}

		s32 execution_type_uniform_location = OpenGL::locate_uniform(program, "execution_type");
		s32 time_uniform_location           = OpenGL::locate_uniform(program, "time");

		OpenGL::uniform_u32_x1(program, "ray_count", ray_count);
		OpenGL::uniform_u32_x1(program, "batch_jump_count", batch_jump_count);

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

			OpenGL::uniform_u32_x1(program, "processed_ray_count", ray_index + 1);

			OpenGL::dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_INITIALIZE);

			for(u32 batch_index = 0; batch_index < batch_count; ++batch_index) {
				OpenGL::uniform_f32_x1(time_uniform_location, (f32)Time::now());
				OpenGL::dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_TRACE);
				jumps_done += batch_jump_count;
			}

			OpenGL::dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_INCLUDE_RAY_COLOR);

			total_ray_time += Time::now() - ray_time_start;

			if(!debug) {
				Window::swap_buffers();
				percent_done   = (f64)jumps_done * 100 / (f64)(ray_count*ray_jump_count);
				estimated_wait = (total_ray_time / (ray_index + 1)) * (ray_count - ray_index - 1);

				Log::percent_done_and_estimated_wait(percent_done, estimated_wait);
			}
		}

		double total_time = Time::now() - time_start;

		if(!debug) {
			std::cout << std::endl;
			Log::measured_timings(total_time, ray_count, batch_count);

			OpenGL::dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_CONVERT_TO_SRGB);
			IO::save_final_output("generated_image.png");
		}
	}

	void debug(u32 ray_count, u32 ray_jump_count, u32 batch_jump_count) {
		s32 time_uniform_location   = OpenGL::locate_uniform(program, "time");
		s32 width_uniform_location  = OpenGL::locate_uniform(program, "width");
		s32 height_uniform_location = OpenGL::locate_uniform(program, "height");

		double time_start;
		double time_end;

		time_start = Time::now();
		while(Window::running())
		{
			if(Window::key_pressed(GLFW_KEY_ESCAPE)) {
				Window::stop_running();
			}

			if(Window::key_pressed(GLFW_KEY_P)) {
				std::this_thread::sleep_for(std::chrono::milliseconds(3000));
			}

			OpenGL::uniform_f32_x1(time_uniform_location, (f32)Time::now());
			OpenGL::uniform_f32_x1(width_uniform_location, (f32)Window::width);
			OpenGL::uniform_f32_x1(height_uniform_location, (f32)Window::height);

			OpenGL::clear_color(0.0f, 0.0f, 0.0f, 1.0f);
			OpenGL::draw_triangles(6);

			run(ray_count, ray_jump_count, batch_jump_count, true);

			Window::swap_buffers();
			Window::poll_events();

			time_end = Time::now();
			Window::set_title(std::to_string(time_end - time_start));
			time_start = time_end;
		}
	}
};

#endif // TRACER_HPP

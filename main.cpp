#include "glad/glad.h"
#include "glfw/glfw3.h"

// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <cstdio>
#include <fstream>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define __ignore__(x)((void)(x))
#define Internal static

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long int u64;
typedef int s32;
typedef long long int s64;
typedef float f32;
typedef double f64;

// NOTE(stekap): Keep track of these global variables so that we don't need to callback
//               glfwGetWindowSize in order to retrieve them from window.
Internal int width = 1000;
Internal int height = 1000;

Internal void framebuffer_size_callback(GLFWwindow* window, int new_width, int new_height) {
	width = new_width;
	height = new_height;
	glViewport(0, 0, width, height);
}

namespace Setup {
	Internal GLFWwindow* window() {
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
		GLFWwindow* window = glfwCreateWindow(width, height, "FragmentShaderPlayground", NULL, NULL);
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	
		if (!window)
		{
			std::cout << "GLFW window creating failed." << std::endl;
			glfwTerminate();
			return nullptr;
		}

		glfwMakeContextCurrent(window);

		if(!gladLoadGL()) {
			std::cout << "GLAD initialization failed." << std::endl;
			glfwTerminate();
			return nullptr;
		}

		glViewport(0, 0, width, height);

		return window;
	}

	Internal void tracer_rectangle() {
		f32 target_rectangle_vertex_data[] = {
			-1.0f, -1.0f,  0.0f,  0.0f,  0.0f,
			1.0f,  1.0f,  0.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
			-1.0f, -1.0f,  0.0f,  0.0f,  0.0f,
			1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
			1.0f,  1.0f,  0.0f,  1.0f,  1.0f,
		};
									
		u32 target_rect_vbo;
		glGenBuffers(1, &target_rect_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, target_rect_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(target_rectangle_vertex_data), target_rectangle_vertex_data, GL_STATIC_DRAW);

		u32 vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(f32), (void*)(0));
	}
}

Internal char* read_entire_text_file(const char* filename) {
	char* data = 0;
	unsigned long size = 0;
		
	FILE* file = fopen(filename, "rb");
	if(file) {
		fseek(file, 0, SEEK_END);
		size = ftell(file);
		data = (char*)malloc(size + 1);
		data[size] = 0;
		fseek(file, 0, SEEK_SET);
	
		if(fread(data, 1, size, file) != size) {
			free(data);
			data = 0;
		}

		fclose(file);
	}
		
	return data;
}

Internal u32 create_shader_program(const char* vertex_shader_path, const char* fragment_shader_path) {
	u32 id = 0;
		
	char* vertex_shader_source = read_entire_text_file(vertex_shader_path);
	if(!vertex_shader_source) {
		std::cout << "Zero pointer provided as vertex shader source path" << std::endl;
		return 0;
	}
	char* fragment_shader_source = read_entire_text_file(fragment_shader_path);
	if(!fragment_shader_source) {
		std::cout << "Zero pointer provided as fragment shader source path" << std::endl;
		return 0;
	}

	u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vertex_shader, 1, &vertex_shader_source, 0);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, 0);

	glCompileShader(vertex_shader);
		
	int valid;
	char info[512];
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &valid);
	if(!valid) {
		glGetShaderInfoLog(vertex_shader, 512, 0, info);
		std::cout << "Vertex shader compilation failed. " << info << std::endl;
	}

	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &valid);
	if(!valid) {
		glGetShaderInfoLog(fragment_shader, 512, 0, info);
		std::cout << "Fragment shader compilation failed. " << info << std::endl;
	}
		
	id = glCreateProgram();
	glAttachShader(id, vertex_shader);
	glAttachShader(id, fragment_shader);
	glLinkProgram(id);
	
	glGetProgramiv(id, GL_LINK_STATUS, &valid);
	if(!valid) {
		glGetProgramInfoLog(id, 512, 0, info);
		std::cout << "Shader program linking failed. " << info << std::endl;
	}

	free(vertex_shader_source);
	free(fragment_shader_source);

	glDetachShader(id, vertex_shader);
	glDetachShader(id, fragment_shader);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return id;
}

Internal void use_shader_program(u32 id) {
	glUseProgram(id);
}

struct V3 { f32 x, y, z; };
struct V4 { f32 x, y, z, w; };

// TODO(stekap): Add camera movement and zoom for debug.
struct Camera {
	V3 p, x, y, z;
	float f;
};

// TODO(stekap): If needed, types that are shared with the shader should be packed better
//               (when their attributes and value ranges become more apparent).

struct Material {
	V3 reflectance;
	f32 scatter;
	V3 emittance;
	f32 SHADER_PAD;

	Material() {}
	Material(V3 reflectance, V3 emittance, f32 scatter)
		: reflectance(reflectance), emittance(emittance), scatter(scatter) {}
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
};

Internal u32 create_uniform_buffer(u64 size_in_bytes) {
	u32 uniform_buffer;
	glGenBuffers(1, &uniform_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
	glBufferData(GL_UNIFORM_BUFFER, size_in_bytes, 0, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	return uniform_buffer;
}

namespace SimpleShaderConfig {
	Internal constexpr u32 max_sphere_count   = 32;
	Internal constexpr u32 max_material_count = 32;
	Internal constexpr u32 max_triangle_count = 32;

	Internal constexpr u32 spheres_ub_bind_index   = 0;
	Internal constexpr u32 triangles_ub_bind_index = 1;
	Internal constexpr u32 materials_ub_bind_index = 2;
};

struct SimpleScene {
	std::vector<Sphere> spheres;
	std::vector<Triangle> triangles;
	std::vector<Material> materials;

	u32 spheres_ub;
	u32 triangles_ub;
	u32 materials_ub;

	SimpleScene() {}
	
	SimpleScene(u32 sphere_count, u32 triangle_count, u32 material_count) {
		assert(sphere_count   <= SimpleShaderConfig::max_sphere_count);
		assert(triangle_count <= SimpleShaderConfig::max_triangle_count);
		assert(material_count <= SimpleShaderConfig::max_material_count);
		
		spheres.resize(sphere_count);
		triangles.resize(triangle_count);
		materials.resize(material_count);
	}

	SimpleScene(std::vector<Sphere>& spheres, std::vector<Triangle>& triangles, std::vector<Material>& materials) {
		assert(spheres.size()   <= SimpleShaderConfig::max_sphere_count);
		assert(triangles.size() <= SimpleShaderConfig::max_triangle_count);
		assert(materials.size() <= SimpleShaderConfig::max_material_count);

		this->spheres = spheres;
		this->triangles = triangles;
		this->materials = materials;
	}

	void create_and_fill_uniform_buffers() {
		spheres_ub   = create_uniform_buffer(SimpleShaderConfig::max_sphere_count   * sizeof(Sphere));
		triangles_ub = create_uniform_buffer(SimpleShaderConfig::max_triangle_count * sizeof(Triangle));
		materials_ub = create_uniform_buffer(SimpleShaderConfig::max_material_count * sizeof(Material));

		glBindBufferRange(GL_UNIFORM_BUFFER, SimpleShaderConfig::spheres_ub_bind_index,   spheres_ub,   0, SimpleShaderConfig::max_sphere_count*sizeof(Sphere));
		glBindBufferRange(GL_UNIFORM_BUFFER, SimpleShaderConfig::triangles_ub_bind_index, triangles_ub, 0, SimpleShaderConfig::max_triangle_count*sizeof(Triangle));
		glBindBufferRange(GL_UNIFORM_BUFFER, SimpleShaderConfig::materials_ub_bind_index, materials_ub, 0, SimpleShaderConfig::max_material_count*sizeof(Material));

		glBindBuffer(GL_UNIFORM_BUFFER, spheres_ub);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, spheres.size()*sizeof(Sphere), spheres.data());
		glBindBuffer(GL_UNIFORM_BUFFER, triangles_ub);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, triangles.size()*sizeof(Triangle), triangles.data());
		glBindBuffer(GL_UNIFORM_BUFFER, materials_ub);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, materials.size()*sizeof(Material), materials.data());
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	static SimpleScene test_scene() {
		std::vector<Sphere> spheres = {
			Sphere({0.0f, 0.0f, -2.0f}, 1.0f, 0),
			Sphere({0.0f, -1000.0f, -2.0f}, 1000.0f, 1),
			Sphere({-2.0f, 2.0f, -4.0f}, 2.0f, 2),
			Sphere({-0.5f, 2.5f, -1.0f}, 0.4f, 3),
		};

		std::vector<Triangle> triangles = {
			Triangle({2.0f, 2.0f, -5.0f}, {3.0f, 0.0f, -2.0f}, {2.5f, 3.0f, -3.5f}, 2),
			Triangle({-1.2f, 0.0f, -2.0f}, {-1.7f, 0.0f, -2.5f}, {-2.0f, 0.0f, -2.0f}, 4),
		};

		std::vector<Material> material = {
			Material({1.0f, 0.4f, 0.3f}, {0.0, 0.0, 0.0}, 0.7f),
			Material({0.3f, 1.0f, 0.3f}, {0.0, 0.0, 0.0}, 0.9f),
			Material({0.7f, 0.7f, 0.7f}, {0.0, 0.0, 0.0}, 0.001f),
			Material({0.8f, 0.8f, 0.8f}, {0.3f, 0.4f, 10.0f}, 0.9f),
			Material({0.6f, 0.2f, 0.2f}, {2.5f, 0.6f, 0.6f}, 0.9f),
		};
		
		SimpleScene scene(spheres, triangles, material);
		scene.create_and_fill_uniform_buffers();
		return scene;
	}

	static SimpleScene cornell_box() {
		std::vector<Sphere> spheres;
		std::vector<Triangle> triangles;
		std::vector<Material> materials;

		// Light
		triangles.push_back(Triangle({343.0f, 548.5f, -227.0f}, {213.0f, 548.5f, -227.2f}, {343.0f, 548.5f, -332.0f}, 3));
		triangles.push_back(Triangle({213.0f, 548.5f, -227.2f}, {213.0f, 548.5f, -332.0f} , {343.0f, 548.5f, -332.0f}, 3));

		// Back wall
		triangles.push_back(Triangle({0.0f, 0.0f, -559.2f}, {556.0f, 0.0f, -559.2f}, {556.0f, 548.8f, -559.2f}, 0));
		triangles.push_back(Triangle({556.0f, 548.8f, -559.2f}, {0.0f, 548.8f, -559.2f}, {0.0f, 0.0f, -559.2f}, 0));

		// Left wall
		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -559.2f}, {0.0f, 548.8f, -559.2f}, 1));
		triangles.push_back(Triangle({0.0f, 548.8f, -559.2f}, {0.0f, 548.8f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1));

		// Right wall
		triangles.push_back(Triangle({556.0f, 0.0f, 0.0f}, {556.0f, 548.8f, 0.0f}, {556.0f, 548.8f, -559.2f}, 2));
		triangles.push_back(Triangle({556.0f, 548.8f, -559.2f}, {556.0f, 0.0f, -559.2f}, {556.0f, 0.0f, 0.0f}, 2));

		// Floor
		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {556.0f, 0.0f, 0.0f}, {556.0f, 0.0f, -559.2f}, 0));
		triangles.push_back(Triangle({556.0f, 0.0f, -559.2f}, {0.0f, 0.0f, -559.2f}, {0.0f, 0.0f, 0.0f}, 0));

		// Ceiling
		triangles.push_back(Triangle({556.0f, 548.8f, 0.0f}, {0.0f, 548.8f, 0.0f}, {556.0f, 548.8f, -559.2f}, 0));
		triangles.push_back(Triangle({0.0f, 548.8f, -559.2f}, {556.0f, 548.8f, -559.2f}, {0.0f, 548.8f, 0.0f}, 0));

		// Short block
		// Top
		triangles.push_back(Triangle({426.0f, 165.0f, -65.0f}, {474.0f, 165.0f, -225.0f}, {316.0f, 165.0f, -272.0f}, 0));
		triangles.push_back(Triangle({266.0f, 165.0f, -114.0f}, {426.0f, 165.0f, -65.0f}, {316.0f, 165.0f, -272.0f}, 0));

		// Left
		triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {266.0f, 165.0f, -114.0f}, {316.0f, 165.0f, -272.0f}, 0));
		triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {316.0f, 165.0f, -272.0f}, {316.0f, 0.0f, -272.0f}, 0));

		// Right
		triangles.push_back(Triangle({474.0f, 0.0f, -225.0f}, {474.0f, 165.0f, -225.0f}, {426.0f, 165.0f, -65.0f}, 0));
		triangles.push_back(Triangle({474.0f, 0.0f, -225.0f}, {426.0f, 165.0f, -65.0f}, {426.0f, 0.0f, -65.0f}, 0));

		// Front
		triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {426.0f, 165.0f, -65.0f}, {266.0f, 165.0f, -114.0f}, 0));
		triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {266.0f, 165.0f, -114.0f}, {266.0f, 0.0f, -114.0f}, 0));

		// Back
		triangles.push_back(Triangle({316.0f, 0.0f, -272.0f}, {316.0f, 165.0f, -272.0f}, {474.0f, 165.0f, -225.0f}, 0));
		triangles.push_back(Triangle({316.0f, 0.0f, -272.0f}, {474.0f, 165.0f, -225.0f}, {474.0f, 0.0f, -225.0f}, 0));
		
		// Tall block
		// Top
		triangles.push_back(Triangle({133.0f, 330.0f, -247.0f}, {291.0f, 330.0f, -296.0f}, {242.0f, 330.0f, -456.0f}, 0));
		triangles.push_back(Triangle({133.0f, 330.0f, -247.0f}, {242.0f, 330.0f, -456.0f}, {84.0f, 330.0f, -406.0f}, 0));

		// Left
		triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {133.0f, 330.0f, -247.0f}, {84.0f, 330.0f, -406.0f}, 0));
		triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {84.0f, 330.0f, -406.0f}, {84.0f, 0.0f, -406.0f}, 0));

		// Right
		triangles.push_back(Triangle({242.0f, 0.0f, -456.0f}, {242.0f, 330.0f, -456.0f}, {291.0f, 330.0f, -296.0f}, 0));
		triangles.push_back(Triangle({242.0f, 0.0f, -456.0f}, {291.0f, 330.0f, -296.0f}, {291.0f, 0.0f, -296.0f}, 0));

		// Front
		triangles.push_back(Triangle({291.0f, 0.0f, -296.0f}, {291.0f, 330.0f, -296.0f}, {133.0f, 330.0f, -247.0f}, 0));
		triangles.push_back(Triangle({291.0f, 0.0f, -296.0f}, {133.0f, 330.0f, -247.0f}, {133.0f, 0.0f, -247.0f}, 0));
		
		// Back
		triangles.push_back(Triangle({84.0f, 0.0f, -406.0f}, {84.0f, 330.0f, -406.0f}, {242.0f, 330.0f, -456.0f}, 0));
		triangles.push_back(Triangle({84.0f, 0.0f, -406.0f}, {242.0f, 330.0f, -456.0f}, {242.0f, 0.0f, -456.0f}, 0));
		
		materials.push_back(Material({0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 0.0f}, 0.95f)); // White
		materials.push_back(Material({0.8f, 0.2f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f)); // Red
		materials.push_back(Material({0.2f, 0.8f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f)); // Green
		materials.push_back(Material({0.6f, 0.6f, 0.6f}, {3.0f, 3.0f, 3.0f}, 0.95f)); // Light

		SimpleScene scene(spheres, triangles, materials);
		scene.create_and_fill_uniform_buffers();
		return scene;
	}
};

enum : u32 {
	EXECUTION_TYPE_INITIALIZE,
	EXECUTION_TYPE_TRACE,
	EXECUTION_TYPE_INCLUDE_RAY_COLOR,
};

Internal void dispatch_batch(s32 execution_type_uniform_location, u32 execution_type) {
	glUniform1ui(execution_type_uniform_location, execution_type);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	// TODO(stekap): Think about this barrier and glFinish further.
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glFinish();
}

u32 create_and_bind_rgba32f_image2d(int image_width, int image_height, u32 image_bind_index) {
	u32 tex;
	glGenTextures(1, &tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, image_width, image_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindImageTexture(image_bind_index, tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	return tex;
}

void save_final_output(std::string image_name) {
	std::vector<u8> pixels(4*width*height);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	stbi_flip_vertically_on_write(true);
	stbi_write_png(image_name.c_str(), width, height, 4, pixels.data(), 4*width);
}

namespace Log {
	void batching_configuration(u32 ray_count, u32 batch_count, u32 ray_jump_count, u32 batch_jump_count) {
		std::cout << "----------------------------------------" << std::endl;
		std::cout << "Ray count              : " << ray_count << std::endl;
		std::cout << "Batch count            : " << batch_count << std::endl;
		std::cout << "Ray jump count         : " << ray_jump_count << std::endl;
		std::cout << "Batch jump count       : " << batch_jump_count << std::endl;
		std::cout << "----------------------------------------" << std::endl;
	}

	void measured_timings(double total_time, u32 ray_count, u32 batch_count) {
		std::cout << "----------------------------------------" << std::endl;
		std::cout << "Total time             : " << total_time << "s" << std::endl;
		std::cout << "Average time per ray   : " << total_time / (ray_count*width*height) << "s" << std::endl;
		std::cout << "Average time per pixel : " << total_time / (width*height) << "s" << std::endl;
		std::cout << "Average time per batch : " << total_time / batch_count << "s" << std::endl;
		std::cout << "----------------------------------------" << std::endl;
	}

	void percent_done(u32 jumps_done, u32 ray_count, u32 ray_jump_count) {
		printf("\rPercent done           : %05.2f%%", (f32)jumps_done * 100 / (f32)(ray_count*ray_jump_count));
		fflush(stdout);
	}
};

void batch_test(GLFWwindow* window, SimpleScene& scene, Camera& camera, u32 batch_progrm) {
	u32 ray_count = 128;
	u32 ray_jump_count = 128;
	u32 batch_jump_count = 128;
	
	u32 batch_count = (ray_jump_count / batch_jump_count);

	Log::batching_configuration(ray_count, batch_count, ray_jump_count, batch_jump_count);
	
	u32 batch_program = create_shader_program("shaders/batch.vert", "shaders/batch.frag");
	use_shader_program(batch_program);

	s32 execution_type_uniform_location = glGetUniformLocation(batch_program, "execution_type");
	s32 time_uniform_location           = glGetUniformLocation(batch_program, "time");

	glUniform1f(glGetUniformLocation(batch_program, "width"), (f32)width);
	glUniform1f(glGetUniformLocation(batch_program, "height"), (f32)height);
	
	glUniform1ui(glGetUniformLocation(batch_program, "ray_count"), ray_count);
	glUniform1ui(glGetUniformLocation(batch_program, "batch_jump_count"), batch_jump_count);
	glUniform1ui(glGetUniformLocation(batch_program, "sphere_count"), (u32)scene.spheres.size());
	glUniform1ui(glGetUniformLocation(batch_program, "triangle_count"), (u32)scene.triangles.size());
		
	glUniform3fv(glGetUniformLocation(batch_program, "camera.p"), 1, (f32*)&camera.p);
	glUniform3fv(glGetUniformLocation(batch_program, "camera.x"), 1, (f32*)&camera.x);
	glUniform3fv(glGetUniformLocation(batch_program, "camera.y"), 1, (f32*)&camera.y);
	glUniform3fv(glGetUniformLocation(batch_program, "camera.z"), 1, (f32*)&camera.z);
	glUniform1f(glGetUniformLocation(batch_program, "camera.f"), camera.f);

	u32 batch_state_texture  = create_and_bind_rgba32f_image2d(4*width, height, 0); __ignore__(batch_state_texture);
	u32 final_colors_texture = create_and_bind_rgba32f_image2d(  width, height, 1); __ignore__(final_colors_texture);
	
	u32 jumps_done = 0;
	double time_start = glfwGetTime();
	for(u32 ray_index = 0; ray_index < ray_count; ++ray_index) {
		glUniform1ui(glGetUniformLocation(batch_program, "processed_ray_count"), ray_index + 1);
		
		glUniform1f(time_uniform_location, (f32)glfwGetTime());
		dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_INITIALIZE);
		
		for(u32 batch_index = 0; batch_index < batch_count; ++batch_index) {
			dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_TRACE);
			jumps_done += batch_jump_count;
		}
		
		dispatch_batch(execution_type_uniform_location, EXECUTION_TYPE_INCLUDE_RAY_COLOR);
		glfwSwapBuffers(window);
		
		Log::percent_done(jumps_done, ray_count, ray_jump_count);
	}

	double total_time = glfwGetTime() - time_start;

	std::cout << std::endl;
	Log::measured_timings(total_time, ray_count, batch_count);
	
	save_final_output("generated_image.png");
}

int main(int arg_count, char** args) {
	GLFWwindow* window = Setup::window();
	
	if(!window) {
		return - 1;
	}

	Setup::tracer_rectangle();

	// u32 base_shader_program = create_shader_program("shaders/base.vert", "shaders/base.frag");
	
	// Cache uniform locations for variables that can change values during execution.
	// s32 time_uniform_location   = glGetUniformLocation(base_shader_program, "time");
	// s32 width_uniform_location  = glGetUniformLocation(base_shader_program, "width");
	// s32 height_uniform_location = glGetUniformLocation(base_shader_program, "height");

#if 0
	SimpleScene test_scene = SimpleScene::test_scene();

	Camera camera = {{0.0f, 1.0f, 1.0f},
					 {1.0f, 0.0f, 0.0f},
					 {0.0f, 1.0f, 0.0f},
					 {0.0f, 0.0f, 1.0f},
					 1.0f};
#else
	SimpleScene test_scene = SimpleScene::cornell_box();

	Camera camera = {{278.0f, 274.0f, 800.0f},
					 {1.0f, 0.0f, 0.0f},
					 {0.0f, 1.0f, 0.0f},
					 {0.0f, 0.0f, 1.0f},
					 1.8f};
#endif

	batch_test(window, test_scene, camera, 0);
	return 0;
	
	// use_shader_program(base_shader_program);

	// glUniform1ui(glGetUniformLocation(base_shader_program, "sphere_count"),   (u32)test_scene.spheres.size());
	// glUniform1ui(glGetUniformLocation(base_shader_program, "triangle_count"), (u32)test_scene.triangles.size());
	
	// glUniform3fv(glGetUniformLocation(base_shader_program, "camera.p"), 1, (f32*)&camera.p);
	// glUniform3fv(glGetUniformLocation(base_shader_program, "camera.x"), 1, (f32*)&camera.x);
	// glUniform3fv(glGetUniformLocation(base_shader_program, "camera.y"), 1, (f32*)&camera.y);
	// glUniform3fv(glGetUniformLocation(base_shader_program, "camera.z"), 1, (f32*)&camera.z);
	// glUniform1f(glGetUniformLocation(base_shader_program, "camera.f"), camera.f);

	// double time_start;
	// double time_end;

	// time_start = glfwGetTime();
	// while(!glfwWindowShouldClose(window))
	// {
	// 	if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
	// 		glfwSetWindowShouldClose(window, true);
	// 	}

	// 	if(glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
	// 		std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	// 	}

	// 	glUniform1f(time_uniform_location, (f32)glfwGetTime());
	// 	glUniform1f(width_uniform_location, (f32)width);
	// 	glUniform1f(height_uniform_location, (f32)height);
			
	// 	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	// 	glClear(GL_COLOR_BUFFER_BIT);

	// 	use_shader_program(base_shader_program);
	// 	glDrawArrays(GL_TRIANGLES, 0, 6);
			
	// 	glfwSwapBuffers(window);
	// 	glfwPollEvents();

	// 	time_end = glfwGetTime();
	// 	glfwSetWindowTitle(window, std::to_string(time_end - time_start).c_str());
	// 	time_start = time_end;
	// }

	glfwTerminate();
	return 0;
}

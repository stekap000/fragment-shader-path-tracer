#include "glad/glad.h"
#include "glfw/glfw3.h"

// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#define __ignore__(x)((void)(x))
#define Internal static

typedef unsigned int u32;
typedef unsigned long long int u64;
typedef int s32;
typedef long long int s64;
typedef float f32;
typedef double f64;

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

struct Camera {
	V3 p, x, y, z;
	float f;
};

struct Material {
	V3 reflectance;
	f32 scatter;
	V3 emittance;
	f32 SHADER_PAD;
};

struct Sphere {
	V3 p;
	f32 r;
	u32 mat_index;
	f32 SHADER_PAD[3];
};

Internal u32 create_uniform_buffer(u64 size_in_bytes) {
	u32 uniform_buffer;
	glGenBuffers(1, &uniform_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
	glBufferData(GL_UNIFORM_BUFFER, size_in_bytes, 0, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	return uniform_buffer;
}

// NOTE(stekap): Keep track of there global variable so that we don't need to callback
//               glfwGetWindowSize in order to retrieve them from window.
Internal int width = 800;
Internal int height = 640;

Internal void framebuffer_size_callback(GLFWwindow* window, int new_width, int new_height) {
	width = new_width;
	height = new_height;
	glViewport(0, 0, width, height);
}

int main() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	GLFWwindow* window = glfwCreateWindow(width, height, "ComputeShaderPlayground", NULL, NULL);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	
	if (!window)
	{
		std::cout << "GLFW window creating failed." << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	if(!gladLoadGL()) {
		std::cout << "GLAD initialization failed." << std::endl;
	}

	glViewport(0, 0, width, height);

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
				
	u32 base_shader_program = create_shader_program("shaders/base.vert", "shaders/base.frag");
	
	// Cache uniform locations for variables that can change values during execution.
	s32 time_uniform_location   = glGetUniformLocation(base_shader_program, "time");
	s32 width_uniform_location  = glGetUniformLocation(base_shader_program, "width");
	s32 height_uniform_location = glGetUniformLocation(base_shader_program, "height");
	
	// NOTE(stekap): Caution when ordering data in uniform buffer because of shader alignment for
	//               struct members and the whole struct itself.
	
	const u32 spheres_ub_bind_index = 0;
	const u32 materials_ub_bind_index = 1;
	const u32 max_sphere_count = 16;
	const u32 max_material_count = 16;
	const u32 sphere_count = 3;
	const u32 material_count = 3;

	Sphere spheres[max_sphere_count] = {
		{{0.0f, 0.0f, -2.0f}, 1.0f, 0},
		{{0.0f, -1000.0f, -2.0f}, 1000.0f, 1},
		{{-2.0f, 2.0f, -4.0f}, 2.0f, 2},
	};

	Material materials[max_material_count] = {
		{{1.0f, 0.4f, 0.3f}, 0.7f, {0.0, 0.0, 0.0}},
		{{0.3f, 1.0f, 0.3f}, 0.9f, {0.0, 0.0, 0.0}},
		{{0.7f, 0.7f, 0.7f}, 0.001f, {0.0, 0.0, 0.0}},
	};

	u32 spheres_ub = create_uniform_buffer(sizeof(spheres));
	u32 materials_ub = create_uniform_buffer(sizeof(materials));

	// NOTE(stekap): From shader side, binding is done explicitly in layout specification for
	//               uniform blocks within shader.
	
	glBindBufferRange(GL_UNIFORM_BUFFER, spheres_ub_bind_index, spheres_ub, 0, sizeof(spheres));
	glBindBufferRange(GL_UNIFORM_BUFFER, materials_ub_bind_index, materials_ub, 0, sizeof(materials));
	
	glBindBuffer(GL_UNIFORM_BUFFER, spheres_ub);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sphere_count*sizeof(Sphere), &spheres[0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBuffer(GL_UNIFORM_BUFFER, materials_ub);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, material_count*sizeof(Material), &materials[0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	Camera camera = {{0.0f, 1.0f, 1.0f},
					 {1.0f, 0.0f, 0.0f},
					 {0.0f, 1.0f, 0.0f},
					 {0.0f, 0.0f, 1.0f},
					 1.0f};

	use_shader_program(base_shader_program);
	
	glUniform1ui(glGetUniformLocation(base_shader_program, "sphere_count"), sphere_count);
	
	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.p"), 1, (f32*)&camera.p);
	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.x"), 1, (f32*)&camera.x);
	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.y"), 1, (f32*)&camera.y);
	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.z"), 1, (f32*)&camera.z);
	glUniform1f(glGetUniformLocation(base_shader_program, "camera.f"), camera.f);

	double time_start;
	double time_end;
	
	time_start = glfwGetTime();
	while(!glfwWindowShouldClose(window))
	{
		if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, true);
		}

		if(glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
			std::this_thread::sleep_for(std::chrono::milliseconds(3000));
		}

		glUniform1f(time_uniform_location, (f32)glfwGetTime());
		glUniform1f(width_uniform_location, (f32)width);
		glUniform1f(height_uniform_location, (f32)height);
			
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
			
		use_shader_program(base_shader_program);
		glDrawArrays(GL_TRIANGLES, 0, 6);
			
		glfwSwapBuffers(window);
		glfwPollEvents();

		time_end = glfwGetTime();
		glfwSetWindowTitle(window, std::to_string(time_end - time_start).c_str());
		time_start = time_end;
	}

	glfwTerminate();
	return 0;
}

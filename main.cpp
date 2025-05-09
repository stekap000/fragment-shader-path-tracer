#include "glad/glad.h"
#include "glfw/glfw3.h"

// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>

#include <iostream>

#define __ignore__(x)((void)(x))
#define Internal static

typedef unsigned int u32;
typedef unsigned long long int u64;
typedef int s32;
typedef long long int s64;
typedef float f32;
typedef double f64;

bool global_engine_running = true;

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

u32 create_shader_program(const char* vertex_shader_path, const char* fragment_shader_path) {
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

struct V3 {
	f32 x;
	f32 y;
	f32 z;
};

struct Camera {
	V3 p;
	V3 x;
	V3 y;
	V3 z;
	float f;
};

struct Sphere {
	V3 p;
	f32 r;
};

int main() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	int width = 800;
	int height = 640;
	GLFWwindow* window = glfwCreateWindow(width, height, "ComputeShaderPlayground", NULL, NULL);
	
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
	s32 time_uniform_location = glGetUniformLocation(base_shader_program, "time");

	Camera camera = {{0.0f, 0.0f, 0.0f},
					 {1.0f, 0.0f, 0.0f},
					 {0.0f, 1.0f, 0.0f},
					 {0.0f, 0.0f, 1.0f},
					 1.0f};

	Sphere sphere = {{0.0f, 0.0f, -2.0f}, 1.0f};

	use_shader_program(base_shader_program);
	glUniform1f(glGetUniformLocation(base_shader_program, "width"), (f32)width);
	glUniform1f(glGetUniformLocation(base_shader_program, "height"), (f32)height);

	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.p"), 1, (f32*)&camera.p);
	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.x"), 1, (f32*)&camera.x);
	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.y"), 1, (f32*)&camera.y);
	glUniform3fv(glGetUniformLocation(base_shader_program, "camera.z"), 1, (f32*)&camera.z);
	glUniform1f(glGetUniformLocation(base_shader_program, "camera.f"), camera.f);

	glUniform3fv(glGetUniformLocation(base_shader_program, "sphere0.p"), 1, (f32*)&sphere.p);
	glUniform1f(glGetUniformLocation(base_shader_program, "sphere0.r"), sphere.r);
	
	glfwGetTime();
	while(!glfwWindowShouldClose(window))
	{
		if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, true);
		}

		glUniform1f(time_uniform_location, (f32)glfwGetTime());

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
					
		use_shader_program(base_shader_program);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		glfwSwapBuffers(window);
		glfwPollEvents();    
	}

	glfwTerminate();
	return 0;
}

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

Internal u32 create_compute_shader_program(const char* compute_shader_path) {
	u32 id = 0;

	char* compute_shader_source = read_entire_text_file(compute_shader_path);
	if(!compute_shader_source) {
		std::cout << "Zero pointer provided as compute shader source path" << std::endl;
		return 0;
	}

	u32 compute_shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute_shader, 1, &compute_shader_source, 0);
	glCompileShader(compute_shader);

	s32 valid;
	char info[512];
	glGetShaderiv(compute_shader, GL_COMPILE_STATUS, &valid);
	if(!valid) {
		glGetShaderInfoLog(compute_shader, 512, 0, info);
		std::cout << "Compute shader compilation failed. " << info << std::endl;
	}

	id = glCreateProgram();
	glAttachShader(id, compute_shader);
	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &valid);
	if(!valid) {
		glGetProgramInfoLog(id, 512, 0, info);
		std::cout << "Compute shader program linking failed. " << info << std::endl;;
	}
	
	free(compute_shader_source);

	glDetachShader(id, compute_shader);
	glDeleteShader(compute_shader);
	
	return id;
}

Internal void use_shader_program(u32 id) {
	glUseProgram(id);
}

struct ComputeGroupLimits {
	s32 count_x = 0;
	s32 count_y = 0;
	s32 count_z = 0;

	s32 size_x = 0;
	s32 size_y = 0;
	s32 size_z = 0;

	s32 local_invocations_count = 0;
	s32 shared_memory_size_in_bytes = 0;

	ComputeGroupLimits() {
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &count_x);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &count_y);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &count_z);

		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &size_x);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &size_y);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &size_z);

		glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &local_invocations_count);
		glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &shared_memory_size_in_bytes);
	}

	void print() {
		std::cout << "WORK GROUP MAX COUNTS:" << std::endl;
		std::cout << "\tx: " << count_x << std::endl;
		std::cout << "\ty: " << count_y << std::endl;
		std::cout << "\tz: " << count_z << std::endl;

		std::cout << "WORK GROUP MAX SIZES:" << std::endl;
		std::cout << "\tx: " << size_x << std::endl;
		std::cout << "\ty: " << size_y << std::endl;
		std::cout << "\tz: " << size_z << std::endl;

		std::cout << "WORK GROUP MAX LOCAL INVOCATIONS COUNT: " << local_invocations_count << std::endl;
		std::cout << "WORK GROUP MAX SHARED MEMORY SIZE (IN BYTES): " << shared_memory_size_in_bytes << std::endl;
	}
};

int main() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	int width = 400;
	int height = 400;
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
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(f32), (void*)(0));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(f32), (void*)(3*sizeof(f32)));

	ComputeGroupLimits compute_group_limits;
	compute_group_limits.print();
				
	const u32 texture_width = 400, texture_height = 400;
	u32 texture;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	// Texture 'texture' is bound to GL_TEXTURE0 texture slot.
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, NULL);
	// Same texture 'texture' is bound to image slot 0 (doesn't need to match texture slot number).
	// Since the actual texture data is the same, and is bound to both texture and image slots, if it is
	// modified by a compute shader, then the changes will also be seen in the main pipeline when the data
	// is accessed by a sampler.
	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
				
	u32 base_shader_program = create_shader_program("shaders/base.vert", "shaders/base.frag");
	u32 compute_shader = create_compute_shader_program("shaders/compute_shader.comp");
				
	// We can also set uniforms externally (currently, they are set internally in shaders).
	// use_shader_program(base_shader_program);
	// glUniform1i(glGetUniformLocation(base_shader_program, "sampler"), 0);
	// use_shader_program(compute_shader);
	// glUniform1i(glGetUniformLocation(compute_shader, "image_output"), 0);
	
	while(!glfwWindowShouldClose(window))
	{
		if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, true);
		}

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
					
		use_shader_program(compute_shader);
		glDispatchCompute((u32)texture_width, (u32)texture_height, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		use_shader_program(base_shader_program);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		glfwSwapBuffers(window);
		glfwPollEvents();    
	}

	glfwTerminate();
	return 0;
}

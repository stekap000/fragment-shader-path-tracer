#ifndef OPENGL_HPP
#define OPENGL_HPP

#include "glad/glad.h"

#include "shared.hpp"
#include "io.hpp"

namespace OpenGL {
	Internal void initialize_tracer_rectangle() {
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

	Internal u32 create_shader_program(const char* vertex_shader_path, const char* fragment_shader_path) {
		u32 id = 0;

		char* vertex_shader_source = IO::read_entire_text_file(vertex_shader_path);
		if(!vertex_shader_source) {
			std::cout << "Zero pointer provided as vertex shader source path" << std::endl;
			return 0;
		}
		char* fragment_shader_source = IO::read_entire_text_file(fragment_shader_path);
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

	Internal u32 create_uniform_buffer(u64 size_in_bytes) {
		u32 uniform_buffer;
		glGenBuffers(1, &uniform_buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
		glBufferData(GL_UNIFORM_BUFFER, size_in_bytes, 0, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		return uniform_buffer;
	}

	Internal void use_shader_program(u32 id) {
		glUseProgram(id);
	}

	Internal u32 create_and_bind_rgba32f_image2d(int image_width, int image_height, u32 image_bind_index) {
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

	Internal s32 locate_uniform(u32 program, std::string_view variable_name) {
		return glGetUniformLocation(program, variable_name.data());
	}

	Internal void uniform_f32_v3(u32 program, std::string_view variable_name, V3 data) {
		glUniform3fv(locate_uniform(program, variable_name), 1, (f32*)&data);
	}

	Internal void uniform_f32_x1(u32 program, std::string_view variable_name, f32 data) {
		glUniform1f(locate_uniform(program, variable_name), data);
	}

	Internal void uniform_f32_x1(u32 uniform_location, f32 data) {
		glUniform1f(uniform_location, data);
	}

	Internal void uniform_u32_x1(u32 program, std::string_view variable_name, u32 data) {
		glUniform1ui(locate_uniform(program, variable_name), data);
	}

	Internal void dispatch_batch(s32 execution_type_uniform_location, u32 execution_type) {
		glUniform1ui(execution_type_uniform_location, execution_type);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		// TODO(stekap): Think about this barrier and glFinish further.
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glFinish();
	}

	Internal void fill_uniform_buffer(u32 uniform_buffer, u64 size_in_bytes, void* data) {
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, size_in_bytes, data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void link_uniform_buffer_to_index(u32 bind_index, u32 uniform_buffer, u64 size_in_bytes) {
		glBindBufferRange(GL_UNIFORM_BUFFER, bind_index, uniform_buffer, 0, size_in_bytes);
	}

	Internal void clear_color(f32 r, f32 g, f32 b, f32 a) {
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	Internal void draw_triangles(u32 count) {
		glDrawArrays(GL_TRIANGLES, 0, count);
	}
};

#endif // OPENGL_HPP

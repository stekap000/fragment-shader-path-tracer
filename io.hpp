#ifndef IO_HPP
#define IO_HPP

#include "glad/glad.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "shared.hpp"
#include "window.hpp"
#include "shader.hpp"

namespace IO {
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

	Internal void save_final_output(std::string image_name) {
		std::vector<u8> pixels(4*Window::width*Window::height);

		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		stbi_flip_vertically_on_write(true);
		stbi_write_png(image_name.c_str(), Window::width, Window::height, 4, pixels.data(), 4*Window::width);
	}

	static std::vector<Triangle> load_obj(std::string filename) {
		tinyobj::ObjReader reader;
		tinyobj::ObjReaderConfig reader_config;
		// Explicit setting to true even though that is the default value.
		reader_config.triangulate = true;

		if(!reader.ParseFromFile(filename, reader_config)) {
			if(!reader.Error().empty()) {
				std::cerr << "TinyObjReader: " << reader.Error() << std::endl;
			}
		}

		if(!reader.Warning().empty()) {
			std::cout << "TinyObjReader: " << reader.Warning() << std::endl;
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();

		size_t triangle_count = 0;
		for(const tinyobj::shape_t& shape : shapes) {
			triangle_count += shape.mesh.indices.size() / 3;
		}

		std::vector<Triangle> triangles(triangle_count);
		size_t triangle_index = 0;

		tinyobj::index_t index_0;
		tinyobj::index_t index_1;
		tinyobj::index_t index_2;
		for(const tinyobj::shape_t& shape : shapes) {
			for(size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
				index_0 = shape.mesh.indices[i + 0];
				index_1 = shape.mesh.indices[i + 1];
				index_2 = shape.mesh.indices[i + 2];

				// First vertex coords.
				triangles[triangle_index].p1 = V3(attrib.vertices[3 * index_0.vertex_index + 0],
												  attrib.vertices[3 * index_0.vertex_index + 1],
												  attrib.vertices[3 * index_0.vertex_index + 2]);
				// Second vertex coords.
				triangles[triangle_index].p2 = V3(attrib.vertices[3 * index_1.vertex_index + 0],
												  attrib.vertices[3 * index_1.vertex_index + 1],
												  attrib.vertices[3 * index_1.vertex_index + 2]);
				// Third vertex coords.
				triangles[triangle_index].p3 = V3(attrib.vertices[3 * index_2.vertex_index + 0],
												  attrib.vertices[3 * index_2.vertex_index + 1],
												  attrib.vertices[3 * index_2.vertex_index + 2]);

				++triangle_index;
			}
		}

		return triangles;
	}
};

#endif // IO_HPP

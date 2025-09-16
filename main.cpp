#include "glad/glad.h"
#include "glfw/glfw3.h"

// NOTE(stekap): We won't bother with resource cleanup like allocated textures or whatever
//               since the program just generates image and exits so the OS will do the cleanup.

#include <iostream>
#include <cstdio>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>
#include <numeric>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define __ignore__(x)((void)(x))
#define Internal static

typedef unsigned char           u8;
typedef unsigned short         u16;
typedef unsigned int           u32;
typedef unsigned long long int u64;
typedef char                    s8;
typedef short                  s16;
typedef int                    s32;
typedef long long int          s64;
typedef float                  f32;
typedef double                 f64;

// NOTE(stekap): Keep track of these global variables so that we don't need to callback
//               glfwGetWindowSize in order to retrieve them from window.
Internal int width  = 800;
Internal int height = 800;

struct V3 {
	f32 x, y, z;

	V3() {}
	V3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}

	V3& operator += (const V3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	V3 operator + (const V3& v) const {
		return V3(x + v.x, y + v.y, z + v.z);
	}

	V3 operator * (const f32 f) const {
		return V3(x*f, y*f, z*f);
	}

	V3& operator *= (const f32 f) {
		x *= f;
		y *= f;
		z *= f;
		return *this;
	}

	V3 operator / (const f32 f) const {
		return V3(x/f, y/f, z/f);
	}

	V3& operator /= (const f32 f) {
		x /= f;
		y /= f;
		z /= f;
		return *this;
	}
};

struct V4 { f32 x, y, z, w; };

// TODO(stekap): Maybe metaprogram part of the shader source, so that we can define things only once on the host
//               and have them in proper form in the shader (for example shared types and some constants like
//               max_triangle_count).

// NOTE(stekap): For types like Material that are shared between this code and shader code,
//               constructor parameters order represents more logical attribute order, but
//               the actual order differs from that in order to be more nicely packed for
//               shader.

// TODO(stekap): If needed, types that are shared with the shader should be packed better
//               (when their attributes and value ranges become more apparent).

enum : u32 {
	MATERIAL_TYPE_NONE       = 0,
	MATERIAL_TYPE_BLACKBODY  = 1,
	MATERIAL_TYPE_DIFFUSE    = 2,
	MATERIAL_TYPE_SPECULAR   = 3,
	MATERIAL_TYPE_DIELECTRIC = 4,
};

struct Material {
	V3 albedo;
	f32 scatter_or_ior;
	V3 emittance;
	// TODO(stekap): Currently, flags is more for testing. Maybe remove, maybe expand.
	u32 type;

	Material() {}
	Material(V3 albedo, V3 emittance, f32 scatter_or_ior, u32 type)
		: albedo(albedo), emittance(emittance), scatter_or_ior(scatter_or_ior), type(type) {}
};

struct Sphere {
	V3 p;
	f32 r;
	u32 mat_index;
	f32 SHADER_PAD[3];

	Sphere() {}
	Sphere(V3 p, f32 r, u32 mat_index) : p(p), r(r), mat_index(mat_index) {}
};


namespace BVH {
	namespace Morton {
		u64 split(u64 x, int log_bits) {
			// NOTE(stekap): For a sequence of bits ...abcdef with length 2^(log_bits), this function produces a sequence where the original
			//               bits are followed by two zeros each i.e. 00.00.00.00a00b00c00d00e00f

			u64 mask = (((u64)1 << ((u64)1 << log_bits)) - 1);
			x &= mask;

			for(int i = log_bits, n = 1 << i; i > 0; --i, n >>= 1) {
				mask = (mask | (mask << n)) & ~(mask << (n / 2));
				x = (x | (x << n)) & mask;
			}

			return x;
		}

		u64 encode(u64 x, u64 y, u64 z, int log_bits) {
			return split(x, log_bits) | (split(y, log_bits) << 1) | (split(z, log_bits) << 2);
		}
	};

	struct AABB {
		f32 min[3];
		f32 max[3];

		float half_area() {
			float edges[] = {max[0] - min[0], max[1] - min[1], max[2] - min[2]};
			return edges[0] * (edges[1] + edges[2]) + edges[1] * edges[2];
		}

		static f32 distance(AABB box1, AABB box2) {
			AABB aabb;

			aabb.min[0] = std::min(box1.min[0], box2.min[0]);
			aabb.min[1] = std::min(box1.min[1], box2.min[1]);
			aabb.min[2] = std::min(box1.min[2], box2.min[2]);

			aabb.max[0] = std::max(box1.max[0], box2.max[0]);
			aabb.max[1] = std::max(box1.max[1], box2.max[1]);
			aabb.max[2] = std::max(box1.max[2], box2.max[2]);

			return aabb.half_area();
		}

		static AABB unionize(AABB box1, AABB box2) {
			return AABB({std::min(box1.min[0], box2.min[0]), std::min(box1.min[1], box2.min[1]), std::min(box1.min[2], box2.min[2])},
						{std::max(box1.max[0], box2.max[0]), std::max(box1.max[1], box2.max[1]), std::max(box1.max[2], box2.max[2])});
		}

		void print() {
			std::cout << "{[" << min[0] << " " << min[1] << " " << min[2] << "], "
					  << "[" << max[0] << " " << max[1] << " " << max[2] << "]}";
		}

		void println() {
			print();
			std::cout << std::endl;
		}
	};

	// TODO(stekap): If we keep only one primitive in the leaf, then there is no need to have the primitive_count field.
	//				 Instead, we can encode whether a node is a leaf using highest bit from children_start_index.
	struct Node {
		AABB aabb;
		u32 children_start_index;
		u32 primitive_count;
	};
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

	inline void translate(V3& translation_vector) {
		p1 += translation_vector;
		p2 += translation_vector;
		p3 += translation_vector;
	}

	V3 centroid() const {
		return (p1 + p2 + p3) / 3;
	}

	BVH::AABB aabb() const {
		BVH::AABB box;

		box.min[0] = std::min(std::min(p1.x, p2.x), p3.x);
		box.min[1] = std::min(std::min(p1.y, p2.y), p3.y);
		box.min[2] = std::min(std::min(p1.z, p2.z), p3.z);

		box.max[0] = std::max(std::max(p1.x, p2.x), p3.x);
		box.max[1] = std::max(std::max(p1.y, p2.y), p3.y);
		box.max[2] = std::max(std::max(p1.z, p2.z), p3.z);

		return box;
	}

	void print() const {
		std::cout << "T{[";
		std::cout << p1.x << " " << p1.y << " " << p1.z << "],";
		std::cout << p2.x << " " << p2.y << " " << p2.z << "],";
		std::cout << p3.x << " " << p3.y << " " << p3.z << "]}";
	}

	void println() const {
		print();
		std::cout << std::endl;
	}
};

struct Camera {
	V3 p, x, y, z;
	float f;

	Camera() {}
	Camera(V3 p, V3 x, V3 y, V3 z, float f) : p(p), x(x), y(y), z(z), f(f) {}

	static Camera test_scene() {
		return Camera({0.0f, 1.0f, 1.0f},
					  {1.0f, 0.0f, 0.0f},
					  {0.0f, 1.0f, 0.0f},
					  {0.0f, 0.0f, 1.0f},
					  1.0f);
	}

	static Camera cornell_box() {
		return Camera({278.0f, 274.0f, 600.0f},
					  {1.0f, 0.0f, 0.0f},
					  {0.0f, 1.0f, 0.0f},
					  {0.0f, 0.0f, 1.0f},
					  2.2f);
	}
};

namespace Time {
	struct Standard {
		u32 h;
		u32 m;
		u32 s;

		Standard() {}

		static Standard from_seconds(u64 seconds) {
			Standard time;
			time.h = (u32)(seconds/3600);
			seconds -= time.h*3600;
			time.m = (u32)(seconds/60);
			seconds -= time.m*60;
			time.s = (u32)seconds;
			return time;
		}
	};

	double now() {
		return glfwGetTime();
	}
};

namespace Log {
	void batching_configuration(u32 ray_count, u32 batch_count, u32 ray_jump_count, u32 batch_jump_count) {
		std::cout << "----------------------------------------" << std::endl;
		std::cout << "Ray count              : " << ray_count << std::endl;
		std::cout << "Batch count            : " << batch_count << std::endl;
		std::cout << "Ray jump count         : " << ray_jump_count << std::endl;
		std::cout << "Batch jump count       : " << batch_jump_count << std::endl;
		std::cout << "----------------------------------------" << std::endl;
	}

	void measured_timings(f64 total_time, u32 ray_count, u32 batch_count) {
		std::cout << "----------------------------------------" << std::endl;
		std::cout << "Total time             : " << total_time << "s" << std::endl;
		std::cout << "Average time per ray   : " << total_time / (ray_count*width*height) << "s" << std::endl;
		std::cout << "Average time per pixel : " << total_time / (width*height) << "s" << std::endl;
		std::cout << "Average time per batch : " << total_time / batch_count << "s" << std::endl;
		std::cout << "----------------------------------------" << std::endl;
	}

	void percent_done_and_estimated_wait(f64 percent_done, f64 estimated_wait) {
		Time::Standard time = Time::Standard::from_seconds((u64)estimated_wait);
		printf("\rPercent done           : %05.2lf%% (Estimated wait time: %02d:%02d:%02d)", percent_done, time.h, time.m, time.s);
		fflush(stdout);
	}
};

namespace Window {
	Internal void framebuffer_size_callback(GLFWwindow* window, int new_width, int new_height) {
		width = new_width;
		height = new_height;
		glViewport(0, 0, width, height);
	}

	Internal GLFWwindow* create(int w, int h, bool visible) {
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_VISIBLE, visible);

		width = w;
		height = h;
	
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
};

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
		std::vector<u8> pixels(4*width*height);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		stbi_flip_vertically_on_write(true);
		stbi_write_png(image_name.c_str(), width, height, 4, pixels.data(), 4*width);
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
};

namespace ShaderConfig {
	Internal constexpr u32 max_sphere_count        = 32;
	Internal constexpr u32 max_triangle_count      = 64;
	Internal constexpr u32 max_material_count      = 32;

	Internal constexpr u32 spheres_ub_bind_index   = 0;
	Internal constexpr u32 triangles_ub_bind_index = 1;
	Internal constexpr u32 materials_ub_bind_index = 2;

	Internal constexpr u32 batch_state_bind_index  = 0;
	Internal constexpr u32 final_colors_bind_index = 1;

	Internal constexpr u32 ray_size_in_vec4        = 5;
};

struct Scene {
	std::vector<Sphere> spheres;
	std::vector<Triangle> triangles;
	std::vector<Material> materials;

	u32 triangle_light_count;
	u32 sphere_light_count;

	Scene() {}

	Scene(u32 sphere_count, u32 sphere_light_count, u32 triangle_count, u32 triangle_light_count, u32 material_count) {
		assert(sphere_count   <= ShaderConfig::max_sphere_count);
		assert(triangle_count <= ShaderConfig::max_triangle_count);
		assert(material_count <= ShaderConfig::max_material_count);
		assert(triangle_light_count <= ShaderConfig::max_triangle_count);
		assert(sphere_light_count <= ShaderConfig::max_sphere_count);
		
		spheres.resize(sphere_count);
		triangles.resize(triangle_count);
		materials.resize(material_count);
	}

	Scene(std::vector<Sphere>& spheres, u32 sphere_light_count, std::vector<Triangle>& triangles, u32 triangle_light_count, std::vector<Material>& materials) {
		assert(spheres.size()   <= ShaderConfig::max_sphere_count);
		assert(triangles.size() <= ShaderConfig::max_triangle_count);
		assert(materials.size() <= ShaderConfig::max_material_count);
		assert(triangle_light_count <= ShaderConfig::max_triangle_count);
		assert(sphere_light_count <= ShaderConfig::max_sphere_count);

		this->spheres = spheres;
		this->triangles = triangles;
		this->materials = materials;
		this->triangle_light_count = triangle_light_count;
		this->sphere_light_count = sphere_light_count;
	}

	static Scene test_scene() {
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
			Material({1.0f, 0.4f, 0.3f}, {0.0, 0.0, 0.0},     0.7f,   MATERIAL_TYPE_DIFFUSE),
			Material({0.3f, 1.0f, 0.3f}, {0.0, 0.0, 0.0},     0.9f,   MATERIAL_TYPE_DIFFUSE),
			Material({0.7f, 0.7f, 0.7f}, {0.0, 0.0, 0.0},     0.001f, MATERIAL_TYPE_DIFFUSE),
			Material({0.8f, 0.8f, 0.8f}, {0.3f, 0.4f, 10.0f}, 0.9f,   MATERIAL_TYPE_BLACKBODY),
			Material({0.6f, 0.2f, 0.2f}, {2.5f, 0.6f, 0.6f},  0.9f,   MATERIAL_TYPE_BLACKBODY),
		};
		
		return Scene(spheres, 1, triangles, 1, material);
	}

	static Scene cornell_box() {
		std::vector<Sphere> spheres;
		std::vector<Triangle> triangles;
		std::vector<Material> materials;

		materials.push_back(Material({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f,   MATERIAL_TYPE_NONE));    // Default material
		materials.push_back(Material({0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE)); // White

		materials.push_back(Material({0.8f, 0.2f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE));    // Red
		materials.push_back(Material({0.2f, 0.8f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.95f,  MATERIAL_TYPE_DIFFUSE));    // Green

		// materials.push_back(Material({0.8f, 0.2f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.005f,  MATERIAL_TYPE_SPECULAR));
		// materials.push_back(Material({0.2f, 0.8f, 0.2f}, {0.0f, 0.0f, 0.0f}, 0.005f,  MATERIAL_TYPE_SPECULAR));

		materials.push_back(Material({0.6f, 0.6f, 0.2f}, {5.0f, 5.0f, 2.5f}, 0.95f,  MATERIAL_TYPE_BLACKBODY));  // Light
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 0.005f, MATERIAL_TYPE_SPECULAR));   // Mirror
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.4f,   MATERIAL_TYPE_DIELECTRIC)); // Cube Glass
		materials.push_back(Material({1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.8f,   MATERIAL_TYPE_DIELECTRIC)); // Sphere Glass

		place_light(triangles, 4);

		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {0.0f, 50.0f, -50.0f}, {0.0f, 50.0f, 0.0f}, 4));
		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -50.0f}, {0.0f, 50.0f, -50.0f}, 4));

		place_back_wall(triangles, 1);
		place_left_wall(triangles, 2);
		place_right_wall(triangles, 3);
		place_floor(triangles, 1);
		place_ceiling(triangles, 1);
		place_short_block(triangles, 1);
		place_tall_block(triangles, 1);

		// spheres.push_back(Sphere({200, 101, -200}, 100.0f, 7));
		// spheres.push_back(Sphere({430, 71, -100}, 70.0f, 7));

		return Scene(spheres, 0, triangles, 4, materials);
	}

	private : static void place_light(std::vector<Triangle>& triangles, int material_index) {
		// Light
		triangles.push_back(Triangle({343.0f, 548.799f, -227.0f}, {213.0f, 548.799f, -227.2f}, {343.0f, 548.799f, -332.0f}, material_index));
		triangles.push_back(Triangle({213.0f, 548.799f, -227.2f}, {213.0f, 548.799f, -332.0f}, {343.0f, 548.799f, -332.0f}, material_index));
	}

	private : static void place_back_wall(std::vector<Triangle>& triangles, int material_index) {
		// Back wall
		triangles.push_back(Triangle({0.0f, 0.0f, -559.2f}, {556.0f, 0.0f, -559.2f}, {556.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({556.0f, 548.8f, -559.2f}, {0.0f, 548.8f, -559.2f}, {0.0f, 0.0f, -559.2f}, material_index));
	}

	private : static void place_left_wall(std::vector<Triangle>& triangles, int material_index) {
		// Left wall
		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -559.2f}, {0.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({0.0f, 548.8f, -559.2f}, {0.0f, 548.8f, 0.0f}, {0.0f, 0.0f, 0.0f}, material_index));
	}

	private : static void place_right_wall(std::vector<Triangle>& triangles, int material_index) {
		// Right wall
		triangles.push_back(Triangle({556.0f, 0.0f, 0.0f}, {556.0f, 548.8f, 0.0f}, {556.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({556.0f, 548.8f, -559.2f}, {556.0f, 0.0f, -559.2f}, {556.0f, 0.0f, 0.0f}, material_index));
	}

	private : static void place_floor(std::vector<Triangle>& triangles, int material_index) {
		// Floor
		triangles.push_back(Triangle({0.0f, 0.0f, 0.0f}, {556.0f, 0.0f, 0.0f}, {556.0f, 0.0f, -559.2f}, material_index));
		triangles.push_back(Triangle({556.0f, 0.0f, -559.2f}, {0.0f, 0.0f, -559.2f}, {0.0f, 0.0f, 0.0f}, material_index));
	}

	private : static void place_ceiling(std::vector<Triangle>& triangles, int material_index) {
		// Ceiling
		triangles.push_back(Triangle({556.0f, 548.8f, 0.0f}, {0.0f, 548.8f, 0.0f}, {556.0f, 548.8f, -559.2f}, material_index));
		triangles.push_back(Triangle({0.0f, 548.8f, -559.2f}, {556.0f, 548.8f, -559.2f}, {0.0f, 548.8f, 0.0f}, material_index));
	}

	private : static void place_short_block(std::vector<Triangle>& triangles, int material_index, V3 translation_vector = {0, 0, 0}) {
		size_t old_triangle_count = triangles.size();

		// Short block
		// Top
		triangles.push_back(Triangle({426.0f, 165.0f, -65.0f}, {474.0f, 165.0f, -225.0f}, {316.0f, 165.0f, -272.0f}, material_index));
		triangles.push_back(Triangle({266.0f, 165.0f, -114.0f}, {426.0f, 165.0f, -65.0f}, {316.0f, 165.0f, -272.0f}, material_index));

		// Left
		triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {266.0f, 165.0f, -114.0f}, {316.0f, 165.0f, -272.0f}, material_index));
		triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {316.0f, 165.0f, -272.0f}, {316.0f, 0.0f, -272.0f}, material_index));

		// Right
		triangles.push_back(Triangle({474.0f, 0.0f, -225.0f}, {474.0f, 165.0f, -225.0f}, {426.0f, 165.0f, -65.0f}, material_index));
		triangles.push_back(Triangle({474.0f, 0.0f, -225.0f}, {426.0f, 165.0f, -65.0f}, {426.0f, 0.0f, -65.0f}, material_index));

		// Front
		triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {426.0f, 165.0f, -65.0f}, {266.0f, 165.0f, -114.0f}, material_index));
		triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {266.0f, 165.0f, -114.0f}, {266.0f, 0.0f, -114.0f}, material_index));

		// Back
		triangles.push_back(Triangle({316.0f, 0.0f, -272.0f}, {316.0f, 165.0f, -272.0f}, {474.0f, 165.0f, -225.0f}, material_index));
		triangles.push_back(Triangle({316.0f, 0.0f, -272.0f}, {474.0f, 165.0f, -225.0f}, {474.0f, 0.0f, -225.0f}, material_index));

		// Bottom
		// triangles.push_back(Triangle({426.0f, 0.0f, -65.0f}, {316.0f, 0.0f, -272.0f}, {474.0f, 0.0f, -225.0f}, material_index));
		// triangles.push_back(Triangle({266.0f, 0.0f, -114.0f}, {316.0f, 0.0f, -272.0f}, {426.0f, 0.0f, -65.0f}, material_index));

		for(size_t i = old_triangle_count; i < old_triangle_count + (triangles.size() - old_triangle_count); ++i) {
			triangles[i].translate(translation_vector);
		}
	}

	private : static void place_tall_block(std::vector<Triangle>& triangles, int material_index, V3 translation_vector = {0, 0, 0}) {
		size_t old_triangle_count = triangles.size();

		// Tall block
		// Top
		triangles.push_back(Triangle({133.0f, 330.0f, -247.0f}, {291.0f, 330.0f, -296.0f}, {242.0f, 330.0f, -456.0f}, material_index));
		triangles.push_back(Triangle({133.0f, 330.0f, -247.0f}, {242.0f, 330.0f, -456.0f}, {84.0f, 330.0f, -406.0f}, material_index));

		// Left
		triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {133.0f, 330.0f, -247.0f}, {84.0f, 330.0f, -406.0f}, material_index));
		triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {84.0f, 330.0f, -406.0f}, {84.0f, 0.0f, -406.0f}, material_index));

		// Right
		triangles.push_back(Triangle({242.0f, 0.0f, -456.0f}, {242.0f, 330.0f, -456.0f}, {291.0f, 330.0f, -296.0f}, material_index));
		triangles.push_back(Triangle({242.0f, 0.0f, -456.0f}, {291.0f, 330.0f, -296.0f}, {291.0f, 0.0f, -296.0f}, material_index));

		// Front
		triangles.push_back(Triangle({291.0f, 0.0f, -296.0f}, {291.0f, 330.0f, -296.0f}, {133.0f, 330.0f, -247.0f}, material_index));
		triangles.push_back(Triangle({291.0f, 0.0f, -296.0f}, {133.0f, 330.0f, -247.0f}, {133.0f, 0.0f, -247.0f}, material_index));

		// Back
		triangles.push_back(Triangle({84.0f, 0.0f, -406.0f}, {84.0f, 330.0f, -406.0f}, {242.0f, 330.0f, -456.0f}, material_index));
		triangles.push_back(Triangle({84.0f, 0.0f, -406.0f}, {242.0f, 330.0f, -456.0f}, {242.0f, 0.0f, -456.0f}, material_index));

		// Bottom
		// triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {242.0f, 0.0f, -456.0f}, {291.0f, 0.0f, -296.0f}, material_index));
		// triangles.push_back(Triangle({133.0f, 0.0f, -247.0f}, {84.0f, 0.0f, -406.0f}, {242.0f, 0.0f, -456.0f}, material_index));

		for(size_t i = old_triangle_count; i < old_triangle_count + (triangles.size() - old_triangle_count); ++i) {
			triangles[i].translate(translation_vector);
		}
	}
};


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
		glUniform1f(glGetUniformLocation(program, "width"), (f32)width);
		glUniform1f(glGetUniformLocation(program, "height"), (f32)height);
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

		u32 batch_state_texture  = OpenGL::create_and_bind_rgba32f_image2d(ShaderConfig::ray_size_in_vec4*width, height, ShaderConfig::batch_state_bind_index);
		__ignore__(batch_state_texture);
		u32 final_colors_texture = OpenGL::create_and_bind_rgba32f_image2d(  width, height, ShaderConfig::final_colors_bind_index);
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
			glUniform1f(width_uniform_location, (f32)width);
			glUniform1f(height_uniform_location, (f32)height);
			
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

void print_morton_bits(u64 morton_code, int log_bits) {
	int bit_count = 3*(1 << log_bits);
	for(int i = bit_count - 1; i >= 0; --i) {
		std::cout << ((morton_code >> i) & 1);
	}
	std::cout << std::endl;
}

std::vector<Triangle> load_test_obj(std::string obj_file) {
	std::vector<Triangle> triangles = IO::load_obj(obj_file);

	for(size_t i = 0; i < triangles.size(); ++i) {
		triangles[i].p1 *= 10000;
		triangles[i].p2 *= 10000;
		triangles[i].p3 *= 10000;
	}

	return triangles;
}

std::pair<std::vector<BVH::Node>, std::vector<u64>> generate_start_nodes_and_sorted_indices(std::vector<Triangle>& triangles) {
	std::vector<BVH::Node> nodes(triangles.size());
	std::vector<u64> morton_codes(triangles.size());

	const u16 world_size = 65535;
	const s16 left_bound = -((world_size + 1)/2);

	for(size_t i = 0; i < nodes.size(); ++i) {
		nodes[i].aabb = triangles[i].aabb();
		nodes[i].children_start_index = (u32)i;
		nodes[i].primitive_count = 1;

		V3 centroid = triangles[i].centroid();
		morton_codes[i] = BVH::Morton::encode((u64)(centroid.x - left_bound),
											  (u64)(centroid.y - left_bound),
											  (u64)(centroid.z - left_bound), 4);
	}

	std::vector<u64> triangle_indices(triangles.size());
	std::iota(triangle_indices.begin(), triangle_indices.end(), 0);

	std::sort(triangle_indices.begin(), triangle_indices.end(), [&morton_codes](u64 x, u64 y){
		return morton_codes[x] < morton_codes[y];
	});

	return std::pair(nodes, triangle_indices);
}

void bvh_construction_test() {
	std::vector<Triangle> triangles = load_test_obj("models/icosahedron.obj");

	// TODO(stekap): BVH linear layout.
	std::vector<BVH::Node> bvh;

	auto [nodes, sorted_indices] = generate_start_nodes_and_sorted_indices(triangles);

	std::vector<u64> neighbours(triangles.size());

	int radius = 5;
	int current_cluster_count = (int)triangles.size();

	while(current_cluster_count > 1) {
		// TODO(stekap): This for loop work should be distributed to multiple threads.
		for(int i = 0; i < current_cluster_count; ++i) {
			float min_distance = std::numeric_limits<float>::max();

			// Finding closest neighbour.
			for(int j = std::max(i - radius, 0); j < std::min(i + radius, current_cluster_count); ++j) {
				float distance = BVH::AABB::distance(nodes[i].aabb, nodes[j].aabb);
				if(i != j && distance < min_distance) {
					min_distance = distance;
					neighbours[i] = j;
				}
			}

			// Merging

			// Compaction

		}
	}
}

// TODO(stekap): Find out why is there a dark edge around the glass sphere.
// TODO(stekap): Check if next_ray is even needed, or it is enough to just use ray.
int main(int arg_count, char** args) {
	bvh_construction_test();

	return 0;

	// False in window creation means that it will be hidden i.e. we will only have console output during generation.
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

	// This generates a single image.
	tracer.run(ray_count, ray_jump_count, batch_jump_count);
	
	// This is a real time mode for debugging. Values like ray_count should be low here.
	// tracer.debug(4, 4, 4);

	glfwTerminate();
	return 0;
}

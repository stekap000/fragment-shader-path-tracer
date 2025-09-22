#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "glad/glad.h"
#include "glfw/glfw3.h"

#include <iostream>
#include <cstdlib>

#include "shared.hpp"

namespace Window {
	Internal GLFWwindow* window;
	Internal int width  = 800;
	Internal int height = 800;

	Internal void framebuffer_size_callback(GLFWwindow* _window, int new_width, int new_height) {
        __ignore__(_window);
		width = new_width;
		height = new_height;
		glViewport(0, 0, width, height);
	}

	Internal void create(int w, int h, bool visible = true) {
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_VISIBLE, visible);

		width = w;
		height = h;

		window = glfwCreateWindow(width, height, "FragmentShaderPlayground", NULL, NULL);
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

		if (!window)
		{
			std::cout << "GLFW window creating failed." << std::endl;
			glfwTerminate();
			std::exit(-1);
		}

		glfwMakeContextCurrent(window);

		if(!gladLoadGL()) {
			std::cout << "GLAD initialization failed." << std::endl;
			glfwTerminate();
			std::exit(-1);
		}

		glViewport(0, 0, width, height);
	}
};

#endif // WINDOW_HPP

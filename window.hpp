#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "glad/glad.h"
#include "glfw/glfw3.h"

#include <iostream>

#include "shared.hpp"

namespace Window {
	Internal int width  = 800;
	Internal int height = 800;

	Internal void framebuffer_size_callback(GLFWwindow* window, int new_width, int new_height) {
        __ignore__(window);
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

#endif // WINDOW_HPP

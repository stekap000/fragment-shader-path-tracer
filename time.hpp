#ifndef TIME_HPP
#define TIME_HPP

#include "glfw/glfw3.h"
#include "shared.hpp"

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

#endif // TIME_HPP

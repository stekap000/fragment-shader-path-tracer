#ifndef TIME_HPP
#define TIME_HPP

#include "glfw/glfw3.h"
#include "shared.hpp"
#include "window.hpp"

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

	struct Measurements {
		f64 total;
		f64 average_per_ray;
		f64 average_per_pixel;
		f64 average_per_batch;

		void update(f64 total_time, u32 ray_count, u32 batch_count) {
			total             = total_time;
			average_per_ray   = total_time / (ray_count*Window::width*Window::height);
			average_per_pixel = total_time / (Window::width*Window::height);
			average_per_batch = total_time / batch_count;
		}
	};

	f64 now() {
		return glfwGetTime();
	}
};

#endif // TIME_HPP

#ifndef LOG_HPP
#define LOG_HPP

#include "window.hpp"
#include "time.hpp"

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
		std::cout << "Average time per ray   : " << total_time / (ray_count*Window::width*Window::height) << "s" << std::endl;
		std::cout << "Average time per pixel : " << total_time / (Window::width*Window::height) << "s" << std::endl;
		std::cout << "Average time per batch : " << total_time / batch_count << "s" << std::endl;
		std::cout << "----------------------------------------" << std::endl;
	}

	void percent_done_and_estimated_wait(f64 percent_done, f64 estimated_wait) {
		Time::Standard time = Time::Standard::from_seconds((u64)estimated_wait);
		printf("\rPercent done           : %05.2lf%% (Estimated wait time: %02d:%02d:%02d)", percent_done, time.h, time.m, time.s);
		fflush(stdout);
	}
};

#endif // LOG_HPP

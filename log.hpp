#ifndef LOG_HPP
#define LOG_HPP

#include "window.hpp"
#include "shared.hpp"
#include "time.hpp"
#include "scene.hpp"
#include "bvh.hpp"

namespace Log {
	void scene_data(Scene& scene) {
		std::cout << "Scene data"                  << std::endl;
		std::cout << "\tTriangle count         : " << scene.triangles.size() << std::endl;
		std::cout << "\tSphere count           : " << scene.spheres.size() << std::endl;
		std::cout << "\tMaterial count         : " << scene.materials.size() << std::endl;
		std::cout << "\tBVH node count         : " << scene.bvh.size() << std::endl;
		std::cout << "\tBVH packed size (MB)   : " << (f32)scene.bvh.size()*sizeof(BVH::PackedNode) / (1 << 20) << std::endl;
		std::cout << std::endl;
	}

	void batching_configuration(u32 ray_count, u32 batch_count, u32 ray_jump_count, u32 batch_jump_count) {
		std::cout << "Batching configuration"      << std::endl;
		std::cout << "\tRay count              : " << ray_count << std::endl;
		std::cout << "\tBatch count            : " << batch_count << std::endl;
		std::cout << "\tRay jump count         : " << ray_jump_count << std::endl;
		std::cout << "\tBatch jump count       : " << batch_jump_count << std::endl;
		std::cout << std::endl;
	}

	void measured_timings(Time::Measurements& time_measurements) {
		std::cout << "Measured timings"            << std::endl;
		std::cout << "\tTotal time             : " << time_measurements.total             << "s" << std::endl;
		std::cout << "\tAverage time per ray   : " << time_measurements.average_per_ray   << "s" << std::endl;
		std::cout << "\tAverage time per pixel : " << time_measurements.average_per_pixel << "s" << std::endl;
		std::cout << "\tAverage time per batch : " << time_measurements.average_per_batch << "s" << std::endl;
		std::cout << std::endl;
	}

	void percent_done_and_estimated_wait(f64 percent_done, f64 estimated_wait) {
		Time::Standard time = Time::Standard::from_seconds((u64)estimated_wait);
		printf("\rPercent done           : %05.2lf%% (Estimated wait time: %02d:%02d:%02d)", percent_done, time.h, time.m, time.s);
		fflush(stdout);
	}
};

#endif // LOG_HPP

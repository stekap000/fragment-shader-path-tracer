#ifndef BVH_HPP
#define BVH_HPP

#include <iostream>
#include <algorithm>
#include <numeric>
#include <vector>
#include <memory>
#include <thread>
#include <barrier>
#include <condition_variable>

#include "shared.hpp"
#include "io.hpp"
#include "shader.hpp"

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

		void print_bits(u64 morton_code, int log_bits) {
			int bit_count = 3*(1 << log_bits);
			for(int i = bit_count - 1; i >= 0; --i) {
				std::cout << ((morton_code >> i) & 1);
			}
			std::cout << std::endl;
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

		static BVH::AABB for_triangle(Triangle t) {
			BVH::AABB box;

			box.min[0] = std::min(std::min(t.p1.x, t.p2.x), t.p3.x);
			box.min[1] = std::min(std::min(t.p1.y, t.p2.y), t.p3.y);
			box.min[2] = std::min(std::min(t.p1.z, t.p2.z), t.p3.z);

			box.max[0] = std::max(std::max(t.p1.x, t.p2.x), t.p3.x);
			box.max[1] = std::max(std::max(t.p1.y, t.p2.y), t.p3.y);
			box.max[2] = std::max(std::max(t.p1.z, t.p2.z), t.p3.z);

			return box;
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
	struct ShaderNode {
		AABB aabb;
		u32 children_start_index;
		u32 primitive_count;
	};

	struct BuildNode {
		AABB aabb;
		u32 first_child_index;
		u32 second_child_index;
	};

	struct Node {
		AABB aabb;
		std::unique_ptr<Node> left_child;
		std::unique_ptr<Node> right_child;
		s32 triangle_index;

		Node() {}
		Node(const AABB& aabb, std::unique_ptr<Node> left_child, std::unique_ptr<Node> right_child, s32 triangle_index = -1)
			: aabb(aabb), left_child(std::move(left_child)), right_child(std::move(right_child)), triangle_index(triangle_index) {}
	};

	std::vector<Triangle> load_test_obj(std::string obj_file) {
		std::vector<Triangle> triangles = IO::load_obj(obj_file);

		for(size_t i = 0; i < triangles.size(); ++i) {
			triangles[i].p1 *= 10000;
			triangles[i].p2 *= 10000;
			triangles[i].p3 *= 10000;
		}

		return triangles;
	}

	std::vector<std::unique_ptr<Node>> generate_sorted_input() {
		std::vector<Triangle> triangles = load_test_obj("models/icosahedron.obj");
		std::vector<Node> nodes(triangles.size());
		std::vector<u64> morton_codes(triangles.size());

		const u16 world_size = 65535;
		const s16 left_bound = -((world_size + 1)/2);

		for(size_t i = 0; i < nodes.size(); ++i) {
			nodes[i].aabb = AABB::for_triangle(triangles[i]);
			nodes[i].left_child = nullptr;
			nodes[i].right_child = nullptr;
			nodes[i].triangle_index = (s32)i;

			V3 centroid = triangles[i].centroid();
			morton_codes[i] = BVH::Morton::encode((u64)(centroid.x - left_bound),
												  (u64)(centroid.y - left_bound),
												  (u64)(centroid.z - left_bound), 4);
		}

		std::vector<u64> sorted_indices(triangles.size());
		std::iota(sorted_indices.begin(), sorted_indices.end(), 0);

		std::sort(sorted_indices.begin(), sorted_indices.end(), [&morton_codes](u64 x, u64 y) {
			return morton_codes[x] < morton_codes[y];
		});

		std::vector<std::unique_ptr<Node>> sorted_input(nodes.size());
		for(size_t i = 0; i < sorted_indices.size(); ++i) {
			sorted_input[i] = std::make_unique<Node>(nodes[sorted_indices[i]].aabb, nullptr, nullptr, nodes[sorted_indices[i]].triangle_index);
		}

		return sorted_input;
	}

	void construct_bvh() {
		std::vector<std::unique_ptr<Node>> in = generate_sorted_input();
		std::vector<std::unique_ptr<Node>> out(in.size());

		int thread_count = std::thread::hardware_concurrency();
		std::vector<std::thread> threads(thread_count);
		std::barrier thread_barrier(thread_count);

		std::vector<int> neighbors(in.size());
		std::vector<int> prefix_sum(in.size() + 1);
		std::vector<int> block_prefix_sum(thread_count);

		int radius = 5;
		int current_cluster_count = (int)in.size();

		std::condition_variable cv;
		bool done = false;

		auto thread_work = [&current_cluster_count, &in, &out, &neighbors, &prefix_sum, &block_prefix_sum, &thread_barrier, &radius, &cv, &done] (int thread_id, int thread_count) {
			if(thread_id == 0) {
				done = false;
			}

			for(int i = thread_id; i < current_cluster_count; i += thread_count) {
				float min_distance = std::numeric_limits<float>::max();

				// Finding closest neighbour.
				for(int j = std::max(i - radius, 0); j < std::min(i + radius, current_cluster_count); ++j) {
					float distance = AABB::distance(in[i]->aabb, in[j]->aabb);
					if(i != j && distance < min_distance) {
						min_distance = distance;
						neighbors[i] = j;
					}
				}
			}

			thread_barrier.arrive_and_wait();

			for(int i = thread_id; i < current_cluster_count; i += thread_count) {
				// Merging
				if(neighbors[neighbors[i]] == i && i < neighbors[i]) {
					in[i] = std::make_unique<Node>(AABB::unionize(in[i]->aabb, in[neighbors[i]]->aabb), std::move(in[i]), std::move(in[neighbors[i]]), -1);
				}
			}

			thread_barrier.arrive_and_wait();

			// Compaction
			// P[i] value incremented if the node is valid i.e. if it is not valid, then it stays the same as P[i-1].
			int clusters_per_thread = (int)in.size() / thread_count;
			int clusters_remainder = in.size() % thread_count;
			int start_index = thread_id * clusters_per_thread;
			if(thread_id == thread_count - 1) {
				clusters_per_thread += clusters_remainder;
			}

			prefix_sum[start_index+1] = (in[start_index] != nullptr);
			for(int i = start_index + 1; i < start_index + clusters_per_thread; ++i) {
				prefix_sum[i+1] = prefix_sum[i] + (in[i] != nullptr);
			}

			thread_barrier.arrive_and_wait();

			// TODO(stekap): This prefix_sum index will not work for the last block if it has more elements than other blocks.
			block_prefix_sum[thread_id] = prefix_sum[thread_id * (in.size() / thread_count)];

			thread_barrier.arrive_and_wait();

			if(thread_id == 0) {
				for(int i = 1; i < thread_count; ++i) {
					block_prefix_sum[i] += block_prefix_sum[i-1];
				}
			}

			thread_barrier.arrive_and_wait();

			for(int i = start_index; i < start_index + clusters_per_thread; ++i) {
				prefix_sum[i+1] += block_prefix_sum[thread_id];
			}

			thread_barrier.arrive_and_wait();

			// Prefix sum is computed at this point.
			// 0 | 1 1 0 1 1 | 0 1 1 1 0 | 1 1  1  1  1 |  1  1  1  0  1
			// 0 | 1 2 2 3 4 | 0 1 2 3 3 | 1 2  3  4  5 |  1  2  3  3  4
			//     0 0 0 0 0 | 4 4 4 4 4 | 3 3  3  3  3 |  5  5  5  5  5
			//     0 0 0 0 0 | 4 4 4 4 4 | 7 7  7  7  7 | 12 12 12 12 12
			// 0 | 1 2 2 3 4 | 4 5 6 7 7 | 8 9 10 11 12 | 13 14 15 15 16

			for(int i = thread_id; i < current_cluster_count; i += thread_count) {
				if(in[i] != nullptr) {
					out[prefix_sum[i+1] - 1] = std::move(in[i]);
				}
			}

			thread_barrier.arrive_and_wait();

			if(thread_id == 0) {
				cv.notify_one();
				done = true;
			}
		};

		std::mutex m;
		std::unique_lock<std::mutex> lock(m);
		while(current_cluster_count > 1) {
			for(int i = 0; i < thread_count; ++i) {
				threads[i] = std::thread(thread_work, i, thread_count);
			}

			cv.wait(lock, [&done](){ return done; });

			for(int i = 0; i < thread_count; ++i) {
				threads[i].join();
			}

			current_cluster_count = prefix_sum[current_cluster_count - 1];
			std::swap(in, out);
		}
	}
};

#endif // BVH_HPP

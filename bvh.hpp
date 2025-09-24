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

		AABB() {}
		AABB(f32 min0, f32 min1, f32 min2, f32 max0, f32 max1, f32 max2) {
			min[0] = min0;
			min[1] = min1;
			min[2] = min2;

			max[0] = max0;
			max[1] = max1;
			max[2] = max2;
		}

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
			return AABB(std::min(box1.min[0], box2.min[0]),
						std::min(box1.min[1], box2.min[1]),
						std::min(box1.min[2], box2.min[2]),
						std::max(box1.max[0], box2.max[0]),
						std::max(box1.max[1], box2.max[1]),
						std::max(box1.max[2], box2.max[2]));
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

	std::vector<std::unique_ptr<Node>> generate_sorted_input(std::vector<Triangle>& triangles) {
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

		// TODO(stekap): Do custom radix sort here.

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

	std::unique_ptr<Node> construct(std::vector<Triangle>& triangles, int radius, int thread_count) {
		std::vector<std::unique_ptr<Node>> in = generate_sorted_input(triangles);
		std::vector<std::unique_ptr<Node>> out(in.size());

		std::vector<std::thread> threads(thread_count);
		std::barrier thread_barrier(thread_count);

		std::vector<int> neighbors(in.size());
		std::vector<int> prefix_sum(in.size() + 1);
		std::vector<int> block_prefix_sum(thread_count);

		int current_cluster_count = (int)in.size();

		std::condition_variable cv;
		bool done = false;

		auto thread_work = [&current_cluster_count, &in, &out, &neighbors, &prefix_sum, &block_prefix_sum, &thread_barrier, &radius, &cv, &done] (int thread_id, int thread_count) {
			done = false;

			int clusters_per_thread = current_cluster_count / thread_count;
			int clusters_remainder = current_cluster_count % thread_count;
			int start_index = thread_id * clusters_per_thread;
			if(thread_id == thread_count - 1) {
				clusters_per_thread += clusters_remainder;
			}

			// Finding closest neighbour.
			for(int i = start_index; i < start_index + clusters_per_thread; ++i) {
				float min_distance = std::numeric_limits<float>::max();

				for(int j = std::max(i - radius, 0); j < std::min(i + radius + 1, current_cluster_count); ++j) {
					float distance = AABB::distance(in[i]->aabb, in[j]->aabb);

					if(i != j && distance < min_distance) {
						min_distance = distance;
						neighbors[i] = j;
					}
				}
			}

			thread_barrier.arrive_and_wait();

			// Merging
			for(int i = start_index; i < start_index + clusters_per_thread; ++i) {
				if(neighbors[neighbors[i]] == i && i < neighbors[i]) {
					in[i] = std::make_unique<Node>(AABB::unionize(in[i]->aabb, in[neighbors[i]]->aabb), std::move(in[i]), std::move(in[neighbors[i]]), -1);
				}
			}

			thread_barrier.arrive_and_wait();

			// Compaction
			prefix_sum[start_index+1] = (in[start_index] != nullptr);
			for(int i = start_index + 1; i < start_index + clusters_per_thread; ++i) {
				prefix_sum[i+1] = prefix_sum[i] + (in[i] != nullptr);
			}

			thread_barrier.arrive_and_wait();

			block_prefix_sum[thread_id] = prefix_sum[thread_id * (current_cluster_count / thread_count)];

			thread_barrier.arrive_and_wait();

			// Sequentially calculate block_prefix_sum (valid for small iteration count i.e. small number of parallel workers).
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

			// At this point, prefix_sum is computed and ready.
			// Example for 20 triangles:
			// validity                  : 0 | 1 1 0 1 1 | 0 1 1 1 0 | 1 1  1  1  1 |  1  1  1  0  1
			// prefix_sum (before block) : 0 | 1 2 2 3 4 | 0 1 2 3 3 | 1 2  3  4  5 |  1  2  3  3  4
			// block_prefix_sum          :   |     0     |     4     |      7       |       12
			// prefix_sum (after block)  : 0 | 1 2 2 3 4 | 4 5 6 7 7 | 8 9 10 11 12 | 13 14 15 15 16

			for(int i = start_index; i < start_index + clusters_per_thread; ++i) {
				if(in[i] != nullptr) {
					out[prefix_sum[i+1] - 1] = std::move(in[i]);
				}
			}

			thread_barrier.arrive_and_wait();

			cv.notify_one();
			done = true;
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

			current_cluster_count = prefix_sum[current_cluster_count];

			std::swap(in, out);
		}

		return std::move(in[0]);
	}

	namespace Test {
		void print_structure(const std::unique_ptr<Node>& bvh, std::string prefix = "") {
			if(bvh != nullptr) {
				if(bvh->triangle_index != -1) {
					std::cout << prefix << bvh->triangle_index << std::endl;
				}
				else {
					std::cout << prefix << "O" << std::endl;
				}
				print_structure(bvh->right_child, prefix + "\t");
				print_structure(bvh->left_child, prefix + "\t");
			}
		}

		void count_leaves(const std::unique_ptr<Node>& bvh, int& count) {
			if(bvh != nullptr) {
				if(bvh->triangle_index != -1) ++count;
				count_leaves(bvh->left_child, count);
				count_leaves(bvh->right_child, count);
			}
		}

		void print_leaf_count(const std::unique_ptr<Node>& bvh) {
			int count = 0;
			count_leaves(bvh, count);
			std::cout << "Leaf count: " << count << std::endl;
		}
	}
};

#endif // BVH_HPP

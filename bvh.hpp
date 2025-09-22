#ifndef BVH_HPP
#define BVH_HPP

#include <iostream>
#include <algorithm>
#include <numeric>
#include <vector>

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

	std::vector<Triangle> load_test_obj(std::string obj_file) {
		std::vector<Triangle> triangles = IO::load_obj(obj_file);

		for(size_t i = 0; i < triangles.size(); ++i) {
			triangles[i].p1 *= 10000;
			triangles[i].p2 *= 10000;
			triangles[i].p3 *= 10000;
		}

		return triangles;
	}

	std::vector<BVH::BuildNode> generate_sorted_start_nodes(std::vector<Triangle>& triangles) {
		std::vector<BVH::BuildNode> nodes(triangles.size());
		std::vector<u64> morton_codes(triangles.size());

		const u16 world_size = 65535;
		const s16 left_bound = -((world_size + 1)/2);

		for(size_t i = 0; i < nodes.size(); ++i) {
			nodes[i].aabb = AABB::for_triangle(triangles[i]);
			nodes[i].first_child_index = (u32)i;
			nodes[i].second_child_index = (u32)i;

			V3 centroid = triangles[i].centroid();
			morton_codes[i] = BVH::Morton::encode((u64)(centroid.x - left_bound),
												  (u64)(centroid.y - left_bound),
												  (u64)(centroid.z - left_bound), 4);
		}

		// TODO(stekap): Use custom radix sort here.

		std::vector<u64> sorted_indices(triangles.size());
		std::iota(sorted_indices.begin(), sorted_indices.end(), 0);

		std::sort(sorted_indices.begin(), sorted_indices.end(), [&morton_codes](u64 x, u64 y) {
			return morton_codes[x] < morton_codes[y];
		});

		std::vector<BVH::BuildNode> sorted_nodes(nodes.size());
		for(size_t i = 0; i < sorted_indices.size(); ++i) {
			sorted_nodes[i] = nodes[sorted_indices[i]];
		}

		return sorted_nodes;
	}

	void construction_test() {
		std::vector<Triangle> triangles = load_test_obj("models/icosahedron.obj");

		// TODO(stekap): BVH linear layout.
		std::vector<BVH::BuildNode> temp_bvh = generate_sorted_start_nodes(triangles);

		// node: aabb, primitive_start_index, primitive_count

		// triangles      : actual triangle data
		// nodes          : leaf nodes of the bvh that only contain one triangle index
		// sorted indices : morton order of the nodes
		// bvh            : actual bvh tree (we want to store node children next to each other)

		std::vector<u64> neighbours(triangles.size());

		int radius = 5;
		int first_cluster_index = 0;
		int current_cluster_count = (int)triangles.size();

		// u32 worker_count = std::thread::hardware_concurrency();
		// std::vector<std::thread> threads(worker_count);
		// std::barrier worker_barrier(worker_count);

		while(current_cluster_count > 1) {
			// TODO(stekap): This for loop work should be distributed to multiple threads.
			for(int i = first_cluster_index; i < current_cluster_count; ++i) {
				float min_distance = std::numeric_limits<float>::max();

				// Finding closest neighbour.
				for(int j = std::max(i - radius, 0); j < std::min(i + radius, current_cluster_count); ++j) {
					float distance = BVH::AABB::distance(temp_bvh[i].aabb, temp_bvh[j].aabb);
					if(i != j && distance < min_distance) {
						min_distance = distance;
						neighbours[i] = j;
					}
				}

				// BARRIER

				// Merging
				// o o _ o o o o o o _ o | M
				// if((int)neighbours[neighbours[i]] == i && i < (int)neighbours[i]) {
				// 	BVH::BuildNode build_node;
				// 	build_node.aabb = BVH::AABB::unionize(temp_bvh[i].aabb, temp_bvh[neighbours[i]].aabb);
				// 	build_node.first_child_index = i;
				// 	build_node.second_child_index = neighbours[i];
				// 	temp_bvh.push_back(build_node);
				// }

				// BARRIER

				// Compaction
				// P[i] value incremented if the node is valid i.e. if it is not valid, then it stays the same as P[i-1].

				// BARRIER
			}
		}
	}
};

#endif // BVH_HPP

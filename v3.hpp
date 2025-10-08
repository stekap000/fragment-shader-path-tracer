#ifndef V3_HPP
#define V3_HPP

#include <cmath>

#include "shared.hpp"

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

	inline void rotate_y(const f32 deg) {
		#define PI 3.14159265358979323846

		f32 rad = (f32)PI*deg/180.0f;

		x = std::cos(rad)*x + std::sin(rad)*z;
		z = -std::sin(rad)*x + std::cos(rad)*z;

		#undef PI
	}
};

#endif // V3_HPP

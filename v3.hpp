#ifndef V3_HPP
#define V3_HPP

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
};

#endif // V3_HPP

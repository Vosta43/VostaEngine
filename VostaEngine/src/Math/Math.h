#pragma once

namespace ve {

	// namespace ve math
	namespace math {

		template<class T>
		constexpr const T& min(const T& a, const T& b) {
			return (b < a) ? b : a;
		}

		template<class T>
		constexpr const T& max(const T& a, const T& b) {
			return (a < b) ? b : a;
		}

		template<class T>
		constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
			return (v < lo) ? lo : (hi < v) ? hi : v;
		}

		inline float smoothstep(float t1, float t2, float x) {
			x = clamp((x - t1) / (t2 - t1), 0.0f, 1.0f);
			return x * x * (3 - 2 * x);
		}


	}
}
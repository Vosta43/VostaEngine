#pragma once
#include <algorithm>

namespace ve {
	
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

	// namespace ve math
	namespace math {
		
		template<class T>
		constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
			return (v < lo) ? lo : (hi < v) ? hi : v;
		}

		float smoothstep(float t1, float t2, float x) {
			x = clamp((x - t1) / (t2 - t1), 0.0f, 1.0f);
			return x * x * (3 - 2 * x);
		}


	}
}
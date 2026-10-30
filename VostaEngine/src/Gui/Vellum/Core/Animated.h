#pragma once

#include <cmath>

namespace ve::vellum {

	// Exponential smoothing toward a target value. The step is
	// `1 - exp(-rate * dt)`, so the visible speed does not depend on the frame
	// rate — the usual `value += (target - value) * k` does.
	//
	// `rate` is in e-folds per second: 10 is mild, 20 is snappy. A host that does
	// not supply time (dt <= 0) gets the end state immediately, so a paused or
	// headless host sees the final look instead of a frozen one.
	struct AnimFloat {
		float value = 0.0f;
		float target = 0.0f;
		float rate = 16.0f;

		void set(float v) { value = target = v; }
		void to(float t) { target = t; }

		// Advances one frame. Returns true while the value is still moving.
		bool tick(float dt) {
			if (dt <= 0.0f || rate <= 0.0f) {
				value = target;
				return false;
			}
			value += (target - value) * (1.0f - std::exp(-rate * dt));
			if (std::fabs(target - value) < 1e-4f) {
				value = target;
				return false;
			}
			return true;
		}
	};

}

#pragma once

#include "Core/Core.h"
#include "Noise/NoiseSettings.h"

namespace ve {

	// CPU evaluation of a NoiseSettings field. The FastNoiseLite backend is
	// private to NoiseCPU.cpp, so no other header in the engine depends on it.
	//
	// Coordinates are world-space; the settings' frequency already carries the
	// scale, so no extra scaling is applied here. The result is remapped from the
	// backend's native [-1, 1] to [outputMin, outputMax].
	//
	// Stateless and thread-safe: each call configures its own backend instance,
	// so a bake or a parallel scatter can share one NoiseSettings freely.
	class VE_API NoiseCPU {
	public:
		static float sample(const NoiseSettings& settings, float x, float y);
		static float sample(const NoiseSettings& settings, float x, float y, float z);
	};

}

#pragma once

#include <glm.hpp>

namespace ve {

	// GPU-facing light block. Kept apart from the reflected Light struct so that
	// shader code that only uploads light data doesn't pull in the reflection and
	// JSON machinery.
	//
	// Must match the std430 layout of `struct Light` in pbrlighting.glsl: vec3
	// aligns to 16 in std430, so the floats that follow each vec3 pack into its
	// tail and the array stride is 48 bytes. The static_asserts below are the
	// guard -- a glm build with GLM_FORCE_ALIGNED_GENTYPES would widen glm::vec3
	// to 16 bytes and silently shift every field after the first vec3.
	struct alignas(16) GpuLightData {
		glm::vec3 position = glm::vec3(0.0f);   // offset  0
		float range = 10.0f;                    // offset 12
		glm::vec3 color = glm::vec3(1.0f);      // offset 16
		float intensity = 1.0f;                 // offset 28
		float attenuationLinear = 0.09f;        // offset 32
		float attenuationQuadratic = 0.032f;    // offset 36
		int type = 0;                           // offset 40 (0 = point)
		int pad0 = 0;                           // offset 44
	};

	static_assert(sizeof(GpuLightData) == 48,
		"GpuLightData must be 48 bytes to match the std430 array stride in pbrlighting.glsl");
	static_assert(offsetof(GpuLightData, range) == 12, "range must follow position");
	static_assert(offsetof(GpuLightData, color) == 16, "color must start a 16-byte block");
	static_assert(offsetof(GpuLightData, intensity) == 28, "intensity must follow color");
	static_assert(offsetof(GpuLightData, type) == 40, "type must sit after the attenuations");

}

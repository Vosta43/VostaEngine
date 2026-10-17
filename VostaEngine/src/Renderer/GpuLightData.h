#pragma once

#include <glm.hpp>

namespace ve {

	// GPU-facing light block. Kept apart from the reflected Light struct so that
	// shader code that only uploads light data doesn't pull in the reflection and
	// JSON machinery.
	struct alignas(16) GpuLightData {
		glm::vec3 position = glm::vec3(0.0f);
		float range = 10.0f;                    // offset 16
		glm::vec3 color = glm::vec3(1.0f);      // offset 32
		float intensity = 1.0f;                // offset 48
		float attenuationLinear = 0.09f;       // offset 52
		float attenuationQuadratic = 0.032f;   // offset 56
		int type = 0;                          // offset 60 (0=point)
		int pad0 = 0;                          // offset 64 (16-byte alignment)
		// Total: 64 bytes (4x16)
	};

}

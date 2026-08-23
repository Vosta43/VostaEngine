#pragma once

#include "Core/Core.h"
#include "Core/Reflection.h"
#include "Scene/Archive.h"

#include <glm.hpp>

namespace ve {
	
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

	VESTRUCT(Light)
	struct Light {
		VEPROPERTY(Light,glm::vec3,position,"position","type=drag")
		glm::vec3 position;
		VEPROPERTY(Light, glm::vec3, color, "color", "type=drag")
		glm::vec3 color = glm::vec3(1.0f);
		VEPROPERTY(Light, float,intensity, "intensity", "type=drag")
		float intensity = 1.0f;
		VEPROPERTY(Light, float,range, "range", "type=drag")
		float range = 10.0f;
		VEPROPERTY(Light, float, attenuationLinear, "attenuationLinear", "type=drag")
		float attenuationLinear = 0.09f;
		VEPROPERTY(Light, float, attenuationQuadratic, "attenuationQuadratic", "type=drag")
		float attenuationQuadratic = 0.032f;

		void serialize(Archive& ar) const {
			ar << position.x << position.y << position.z 
			   << color.x    << color.y    << color.z
			   << intensity << range << attenuationLinear <<attenuationQuadratic;
		}
		void deserialize(Archive& ar) {
			ar >> position.x >> position.y >> position.z
				>> color.r >> color.g >> color.b
				>> intensity >> range >> attenuationLinear >> attenuationQuadratic;
		}

	};

}
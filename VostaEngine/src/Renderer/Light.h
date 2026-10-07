#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Core/Reflection.h"

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

		void serialize(JsonWriter& w) const {
			w.set("position", position);
			w.set("color", color);
			w.set("intensity", intensity);
			w.set("range", range);
			w.set("attenuationLinear", attenuationLinear);
			w.set("attenuationQuadratic", attenuationQuadratic);
		}

		void deserialize(const JsonReader& r) {
			position = r.getVec3("position", position);
			color = r.getVec3("color", color);
			intensity = r.getFloat("intensity", intensity);
			range = r.getFloat("range", range);
			attenuationLinear = r.getFloat("attenuationLinear", attenuationLinear);
			attenuationQuadratic = r.getFloat("attenuationQuadratic", attenuationQuadratic);
		}

	};

}
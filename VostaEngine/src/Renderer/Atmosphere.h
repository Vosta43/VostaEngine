#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Core/Reflection.h"

#include <glm.hpp>

namespace ve {

	// Physically-based atmospheric scattering parameters (Rayleigh + Mie).
	// Sea-level scattering coefficients and scale heights follow standard
	// Earth-like reference values (Bruneton 2008 / Hillaire 2020).
	VESTRUCT(AtmosphereParams)
	struct AtmosphereParams {
		VEPROPERTY(AtmosphereParams, glm::vec3, sunDirection, "Sun Direction", "type=drag")
		glm::vec3 sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);

		VEPROPERTY(AtmosphereParams, float, sunIntensity, "Sun Intensity", "type=drag,minValue=0,maxValue=100,speed=0.01")
		float sunIntensity = 4.5f;

		VEPROPERTY(AtmosphereParams, glm::vec3, rayleighScattering, "Rayleigh Scattering", "type=drag")
		glm::vec3 rayleighScattering = glm::vec3(5.802e-6f, 13.558e-6f, 33.1e-6f);

		VEPROPERTY(AtmosphereParams, float, rayleighScaleHeight, "Rayleigh Scale Height", "type=drag,minValue=100,maxValue=50000")
		float rayleighScaleHeight = 8000.0f;

		VEPROPERTY(AtmosphereParams, glm::vec3, mieScattering, "Mie Scattering", "type=drag")
		glm::vec3 mieScattering = glm::vec3(3.996e-6f);

		VEPROPERTY(AtmosphereParams, float, mieScaleHeight, "Mie Scale Height", "type=drag,minValue=100,maxValue=10000")
		float mieScaleHeight = 1200.0f;

		VEPROPERTY(AtmosphereParams, float, miePhaseG, "Mie Phase G", "type=drag,minValue=-1,maxValue=1")
		float miePhaseG = 0.8f;

		VEPROPERTY(AtmosphereParams, glm::vec3, groundAlbedo, "Ground Albedo", "type=drag")
		glm::vec3 groundAlbedo = glm::vec3(0.1f);

		VEPROPERTY(AtmosphereParams, float, planetRadius, "Planet Radius", "type=drag,minValue=1000,maxValue=10000000")
		float planetRadius = 6371000.0f;

		VEPROPERTY(AtmosphereParams, float, atmosphereHeight, "Atmosphere Height", "type=drag,minValue=1000,maxValue=200000")
		float atmosphereHeight = 80000.0f;

		VEPROPERTY(AtmosphereParams, float, multipleScattering, "Multiple Scattering", "type=drag,minValue=0,maxValue=1")
		float multipleScattering = 0.5f;

		VEPROPERTY(AtmosphereParams, float, exposure, "Exposure", "type=drag,minValue=0,maxValue=10")
		float exposure = 1.0f;

		void serialize(JsonWriter& w) const {
			w.set("sunDirection", sunDirection);
			w.set("sunIntensity", sunIntensity);
			w.set("rayleighScattering", rayleighScattering);
			w.set("rayleighScaleHeight", rayleighScaleHeight);
			w.set("mieScattering", mieScattering);
			w.set("mieScaleHeight", mieScaleHeight);
			w.set("miePhaseG", miePhaseG);
			w.set("groundAlbedo", groundAlbedo);
			w.set("planetRadius", planetRadius);
			w.set("atmosphereHeight", atmosphereHeight);
			w.set("multipleScattering", multipleScattering);
			w.set("exposure", exposure);
		}

		void deserialize(const JsonReader& r) {
			sunDirection = r.getVec3("sunDirection", sunDirection);
			sunIntensity = r.getFloat("sunIntensity", sunIntensity);
			rayleighScattering = r.getVec3("rayleighScattering", rayleighScattering);
			rayleighScaleHeight = r.getFloat("rayleighScaleHeight", rayleighScaleHeight);
			mieScattering = r.getVec3("mieScattering", mieScattering);
			mieScaleHeight = r.getFloat("mieScaleHeight", mieScaleHeight);
			miePhaseG = r.getFloat("miePhaseG", miePhaseG);
			groundAlbedo = r.getVec3("groundAlbedo", groundAlbedo);
			planetRadius = r.getFloat("planetRadius", planetRadius);
			atmosphereHeight = r.getFloat("atmosphereHeight", atmosphereHeight);
			multipleScattering = r.getFloat("multipleScattering", multipleScattering);
			exposure = r.getFloat("exposure", exposure);
		}
	};

}

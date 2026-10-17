#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Core/Reflection.h"

#include <glm.hpp>

namespace ve {

	// CIE xy Planckian-locus fit (Kim et al.) -> linear sRGB, normalized so the
	// brightest channel is 1. Gives a blackbody its chromaticity at constant
	// apparent brightness, so switching color models never changes exposure.
	inline glm::vec3 blackbodyRGB(float kelvin) {
		const float T = glm::clamp(kelvin, 1000.0f, 40000.0f);
		float x;
		if (T <= 4000.0f)
			x = -0.2661239e9f / (T*T*T) - 0.2343589e6f / (T*T) + 0.8776956e3f / T + 0.179910f;
		else
			x = -3.0258469e9f / (T*T*T) + 2.1070379e6f / (T*T) + 0.2226347e3f / T + 0.240390f;

		float y;
		if (T <= 2222.0f)      y = -1.1063814f*x*x*x - 1.34811020f*x*x + 2.18555832f*x - 0.20219683f;
		else if (T <= 4000.0f) y = -0.9549476f*x*x*x - 1.37418593f*x*x + 2.09137015f*x - 0.16748867f;
		else                   y =  3.0817580f*x*x*x - 5.87338670f*x*x + 3.75112997f*x - 0.37001483f;

		const float X = x / y;
		const float Z = (1.0f - x - y) / y;
		glm::vec3 rgb(                         // Y = 1
			 3.2406f * X - 1.5372f - 0.4986f * Z,
			-0.9689f * X + 1.8758f + 0.0415f * Z,
			 0.0557f * X - 0.2040f + 1.0570f * Z);
		rgb = glm::max(rgb, glm::vec3(0.0f));
		const float m = glm::max(rgb.r, glm::max(rgb.g, rgb.b));
		return m > 0.0f ? rgb / m : glm::vec3(1.0f);
	}

	// Physically-based atmospheric scattering parameters (Rayleigh + Mie).
	// Sea-level scattering coefficients and scale heights follow standard
	// Earth-like reference values (Bruneton 2008 / Hillaire 2020).
	VESTRUCT(AtmosphereParams)
	struct AtmosphereParams {
		VEPROPERTY(AtmosphereParams, glm::vec3, sunDirection, "Sun Direction", "type=drag")
		glm::vec3 sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);

		VEPROPERTY(AtmosphereParams, float, sunIntensity, "Sun Intensity", "type=drag,minValue=0,maxValue=100,speed=0.01")
		float sunIntensity = 4.5f;

		// Sun emitter color: either an explicit tint or a blackbody temperature.
		// useTemperature picks the model (UE-style "Use Temperature"); its product
		// with sunIntensity is the radiance the shaders consume.
		VEPROPERTY(AtmosphereParams, bool, useTemperature, "Use Temperature", "type=bool")
		bool useTemperature = false;

		VEPROPERTY(AtmosphereParams, glm::vec3, sunColor, "Sun Color", "type=drag")
		glm::vec3 sunColor = glm::vec3(1.0f);

		VEPROPERTY(AtmosphereParams, float, sunTemperature, "Sun Temperature", "type=drag,minValue=1000,maxValue=40000,speed=10")
		float sunTemperature = 5778.0f;

		// The emitter tint for the active model: sunColor verbatim, or a
		// normalized blackbody chromaticity when useTemperature is set.
		glm::vec3 resolveSunColor() const {
			return useTemperature ? blackbodyRGB(sunTemperature) : sunColor;
		}

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
			w.set("useTemperature", useTemperature);
			w.set("sunColor", sunColor);
			w.set("sunTemperature", sunTemperature);
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
			useTemperature = r.getBool("useTemperature", useTemperature);
			sunColor = r.getVec3("sunColor", sunColor);
			sunTemperature = r.getFloat("sunTemperature", sunTemperature);
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

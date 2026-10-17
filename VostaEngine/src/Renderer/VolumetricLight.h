#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Core/Reflection.h"

namespace ve {

	// Sun-only screen-space volumetric light (god rays / Tyndall): a shadow-mapped
	// ray march through a homogeneous participating medium. Each sample tests the
	// cascaded shadow maps, so shafts appear only where geometry occludes the sun.
	// Distances are in meters, matching the scene.
	VESTRUCT(VolumetricLightParams)
	struct VolumetricLightParams {
		// Extinction coefficient of the medium (sigma_e, per meter). Higher =
		// thicker haze and brighter shafts.
		VEPROPERTY(VolumetricLightParams, float, density, "Density", "type=drag,minValue=0,maxValue=1,speed=0.001")
		float density = 0.02f;

		// Single-scattering albedo: the fraction of extinguished light that
		// scatters toward the camera instead of being absorbed.
		VEPROPERTY(VolumetricLightParams, float, scattering, "Scattering", "type=drag,minValue=0,maxValue=1,speed=0.001")
		float scattering = 1.0f;

		// Henyey-Greenstein asymmetry g. >0 forward-scatters (brightest looking
		// toward the sun), 0 is isotropic. Capped below ~0.95 to avoid a
		// single-sample hotspot.
		VEPROPERTY(VolumetricLightParams, float, anisotropy, "Anisotropy", "type=drag,minValue=-0.9,maxValue=0.9,speed=0.01")
		float anisotropy = 0.8f;

		// Ray march length in meters. Only the near field is sampled; beyond this
		// the medium contributes nothing, which keeps the step count affordable.
		VEPROPERTY(VolumetricLightParams, float, maxDistance, "Max Distance", "type=drag,minValue=1,maxValue=500,speed=1")
		float maxDistance = 120.0f;

		VEPROPERTY(VolumetricLightParams, int, stepCount, "Step Count", "type=drag,speed=1,minValue=4,maxValue=64")
		int stepCount = 16;

		// Final brightness scale on the accumulated in-scatter.
		VEPROPERTY(VolumetricLightParams, float, intensity, "Intensity", "type=drag,minValue=0,maxValue=4,speed=0.01")
		float intensity = 1.0f;

		// Exponential height falloff of the medium (per meter) and the altitude
		// where density is authored. 0 falloff = uniform fog.
		VEPROPERTY(VolumetricLightParams, float, heightFalloff, "Height Falloff", "type=drag,minValue=0,maxValue=1,speed=0.001")
		float heightFalloff = 0.0f;

		VEPROPERTY(VolumetricLightParams, float, baseHeight, "Base Height", "type=drag,minValue=-1000,maxValue=10000,speed=1")
		float baseHeight = 0.0f;

		// Temporal history weight when accumulating the half-res buffer. Higher =
		// smoother but more ghosting on motion.
		VEPROPERTY(VolumetricLightParams, float, temporalBlend, "Temporal Blend", "type=drag,minValue=0,maxValue=0.99,speed=0.01")
		float temporalBlend = 0.9f;

		void serialize(JsonWriter& w) const {
			w.set("density", density);
			w.set("scattering", scattering);
			w.set("anisotropy", anisotropy);
			w.set("maxDistance", maxDistance);
			w.set("stepCount", stepCount);
			w.set("intensity", intensity);
			w.set("heightFalloff", heightFalloff);
			w.set("baseHeight", baseHeight);
			w.set("temporalBlend", temporalBlend);
		}

		void deserialize(const JsonReader& r) {
			density = r.getFloat("density", density);
			scattering = r.getFloat("scattering", scattering);
			anisotropy = r.getFloat("anisotropy", anisotropy);
			maxDistance = r.getFloat("maxDistance", maxDistance);
			stepCount = r.getInt("stepCount", stepCount);
			intensity = r.getFloat("intensity", intensity);
			heightFalloff = r.getFloat("heightFalloff", heightFalloff);
			baseHeight = r.getFloat("baseHeight", baseHeight);
			temporalBlend = r.getFloat("temporalBlend", temporalBlend);
		}
	};

}

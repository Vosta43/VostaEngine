#pragma once

#include "Core/Core.h"
#include "Core/Reflection.h"
#include "Scene/Archive.h"

namespace ve {

	// Physically-based volumetric cloud layer parameters.
	// Altitudes are measured above the planet surface (sea level); the absolute
	// sphere radii are derived from AtmosphereParams::planetRadius at upload time.
	VESTRUCT(CloudParams)
	struct CloudParams {
		VEPROPERTY(CloudParams, float, bottomAltitude, "Bottom Altitude", "type=drag,minValue=0,maxValue=20000")
		float bottomAltitude = 1500.0f;

		VEPROPERTY(CloudParams, float, topAltitude, "Top Altitude", "type=drag,minValue=0,maxValue=20000")
		float topAltitude = 3800.0f;

		// View-side extinction along the camera ray (Unity _lightAbsorptionThroughCloud).
		VEPROPERTY(CloudParams, float, sigma, "Extinction", "type=drag,minValue=0,maxValue=1,speed=0.001")
		float sigma = 0.128f;

		// Ambient skylight floor: keeps cloud shadows from going fully black
		// (uniform u_CloudAmbient in volume_cloud.glsl).
		VEPROPERTY(CloudParams, float, ambient, "Ambient", "type=drag,minValue=0,maxValue=1,speed=0.001")
		float ambient = 0.01f;

		VEPROPERTY(CloudParams, float, coverage, "Coverage", "type=drag,minValue=0,maxValue=1")
		float coverage = 0.42f;

		// Shape-noise remap bounds [shapeMin, shapeMax] -> [0,1]: bracket what the
		// shape noise produces so the top saturates to solid cloud and the bottom
		// fades out (uniforms u_CloudShapeMin/u_CloudShapeMax).
		VEPROPERTY(CloudParams, float, shapeMin, "Shape Min", "type=drag,minValue=0,maxValue=1")
		float shapeMin = 0.35f;

		VEPROPERTY(CloudParams, float, shapeMax, "Shape Max", "type=drag,minValue=0,maxValue=1")
		float shapeMax = 0.90f;

		// Step of 1e-5 and 6-decimal display: the noise frequency lives in a tiny
		// [0,0.01] range where the default "%.3f" (3 decimals) swallowed fine values.
		VEPROPERTY(CloudParams, float, baseFrequency, "Base Frequency", "type=drag,minValue=0,maxValue=0.01,speed=0.00001,format=%.6f")
		float baseFrequency = 0.00028f;

		// Fixed march step in meters — art-tunable, decoupled from baseFrequency
		// (Unity-style). Ray reach = MAX_STEPS * step; smaller steps resolve more
		// detail but cap reach sooner on grazing rays.
		VEPROPERTY(CloudParams, float, stepSize, "March Step", "type=drag,minValue=30,maxValue=500")
		float stepSize = 128.0f;

		// Per-octave FBM weights for the shape texture. Normalized to sum 1 in the
		// shader, so only the ratios matter, not the absolute scale.
		VEPROPERTY(CloudParams, glm::vec4, shapeNoiseWeights, "Shape Weights", "type=drag,minValue=0,maxValue=1")
		glm::vec4 shapeNoiseWeights = glm::vec4(0.5f, 0.93f, 0.38f, 0.67f);

		// 3-stop tone ramp colors (Unity-style lightmarch color map), driving the
		// u_CloudShadowColor/u_CloudMidColor uniforms in volume_cloud.glsl. The two
		// offsets position the mid->lit and shadow->mid transitions.
		VEPROPERTY(CloudParams, glm::vec3, shadowColor, "Shadow Color", "type=drag")
		glm::vec3 shadowColor = glm::vec3(0.08f, 0.10f, 0.18f);   // cool blue-gray, linear

		VEPROPERTY(CloudParams, glm::vec3, midColor, "Mid Color", "type=drag")
		glm::vec3 midColor = glm::vec3(0.30f, 0.26f, 0.20f);      // warm gray, linear

		VEPROPERTY(CloudParams, float, rampOffset1, "Ramp Offset 1", "type=drag,minValue=0.1,maxValue=10,speed=0.01")
		float rampOffset1 = 1.7f;

		VEPROPERTY(CloudParams, float, rampOffset2, "Ramp Offset 2", "type=drag,minValue=0.1,maxValue=10,speed=0.01")
		float rampOffset2 = 1.6f;

		// Light-side absorption toward the sun (Unity _lightAbsorptionTowardSun):
		// how much cloud between the sample and the sun blocks the light (self-
		// shadow depth). Independent of sigma, which stays the view-side through-
		// cloud absorption. Default 2.0 * sigma matches the old hardcoded factor.
		VEPROPERTY(CloudParams, float, lightAbsorption, "Light Absorption", "type=drag,minValue=0,maxValue=1,speed=0.001")
		float lightAbsorption = 0.003f;

		// Detail-erosion strength (Unity _detailNoiseWeight): scales the detail
		// FBM term in applyDetail. Higher = sharper crevices; too high aliases
		// edges. Hillaire uses 0.2.
		VEPROPERTY(CloudParams, float, detailErodeWeight, "Detail Erode Weight", "type=drag,minValue=0,maxValue=1,speed=0.001")
		float detailErodeWeight = 0.292f;

		// Final density scale (Unity _densityMultiplier): multiplies the eroded
		// density before clamping. <1 thins the whole cloud, >1 fattens it.
		VEPROPERTY(CloudParams, float, densityMultiplier, "Density Multiplier", "type=drag,minValue=0,maxValue=10,speed=0.01")
		float densityMultiplier = 1.3f;

		// Wind advection: the sample position is translated horizontally by
		// windDirection * windSpeed * time (see sampleShape in volume_cloud.glsl).
		// windDirection is a horizontal (x,z) direction, normalized before scaling;
		// 0 speed = static deck.
		VEPROPERTY(CloudParams, float, windSpeed, "Wind Speed", "type=drag,minValue=0,maxValue=500,speed=1")
		float windSpeed = 0.0f;

		VEPROPERTY(CloudParams, glm::vec2, windDirection, "Wind Direction", "type=drag")
		glm::vec2 windDirection = glm::vec2(1.0f, 0.0f);

		// Unity HDRP "height weights": lerps the stratus (0, thin low sheet) and
		// cumulus (1, tall flat-topped slab) vertical profiles in heightGradient
		// (volume_cloud.glsl) — the cloud-type morph. The band itself is positioned
		// by the weather map's coverage (u_WeatherMap R * u_CloudCoverage).
		VEPROPERTY(CloudParams, float, heightGradientWeight, "Height Gradient Weight", "type=drag,minValue=0,maxValue=1")
		float heightGradientWeight = 0.5f;

		// Detail-noise frequency as a multiple of the shape frequency (Unity
		// _detailTiling / _shapeTiling ratio, uniform u_CloudDetailFrequency). Must
		// stay > 1 so the erosion resolves finer than the base forms; default 8
		// preserves the previous hardcoded DETAIL_FREQ_SCALE.
		VEPROPERTY(CloudParams, float, detailFrequency, "Detail Frequency", "type=drag,minValue=1,maxValue=64,speed=0.5")
		float detailFrequency = 8.0f;

		// Per-layer wind, relative to the master wind (windSpeed/windDirection):
		// detailWindScale scrolls the detail field OVER the shape (1 = locked to the
		// shape advection, >1 = detail runs ahead — turbulence-scale eddies move
		// faster than the bulk cloud); weatherWindScale drifts the coverage pattern
		// at a fraction of the wind's angular speed around the cloud layer.
		VEPROPERTY(CloudParams, float, detailWindScale, "Detail Wind Scale", "type=drag,minValue=0,maxValue=10,speed=0.05")
		float detailWindScale = 1.5f;

		VEPROPERTY(CloudParams, float, weatherWindScale, "Weather Wind Scale", "type=drag,minValue=0,maxValue=2,speed=0.05")
		float weatherWindScale = 0.4f;

		void serialize(Archive& ar) const {
			ar << bottomAltitude << topAltitude << sigma << ambient << coverage << baseFrequency << stepSize << shapeNoiseWeights
			   << shadowColor << midColor << rampOffset1 << rampOffset2 << lightAbsorption << detailErodeWeight << densityMultiplier
			   << windSpeed << windDirection << shapeMin << shapeMax << heightGradientWeight << detailFrequency << detailWindScale << weatherWindScale;
		}

		void deserialize(Archive& ar) {
			ar >> bottomAltitude >> topAltitude >> sigma >> ambient >> coverage >> baseFrequency >> stepSize >> shapeNoiseWeights
			   >> shadowColor >> midColor >> rampOffset1 >> rampOffset2 >> lightAbsorption >> detailErodeWeight >> densityMultiplier
			   >> windSpeed >> windDirection >> shapeMin >> shapeMax >> heightGradientWeight >> detailFrequency >> detailWindScale >> weatherWindScale;
		}
	};

}

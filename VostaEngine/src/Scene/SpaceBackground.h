#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Core/Reflection.h"

#include <glm.hpp>

namespace ve {

	// Stars live on a cube-sphere grid whose stars sit at cell centres. One hash
	// per pixel per layer finds the layer's star in that cell: fixed positions mean
	// a PSF never straddles a cell boundary, and no star catalogue is needed. The
	// field is a pure function of world direction, so it never parallaxes and does
	// not follow the planet's rotation.
	VESTRUCT(SpaceStarsParams)
	struct SpaceStarsParams {
		VEPROPERTY(SpaceStarsParams, int, layers, "Layers", "type=drag,minValue=1,maxValue=8,speed=1")
		int layers = 4;

		// Cells per cube face, per layer.
		VEPROPERTY(SpaceStarsParams, float, density, "Density (cells per face)", "type=drag,minValue=4,maxValue=512,speed=1")
		float density = 64.0f;

		VEPROPERTY(SpaceStarsParams, float, brightness, "Brightness", "type=drag,minValue=0,maxValue=100,speed=0.01")
		float brightness = 1.0f;

		// Angular radius in radians. The shader raises it to 1.5 pixels when the
		// footprint is larger, so sub-pixel stars do not flicker under TAA jitter.
		VEPROPERTY(SpaceStarsParams, float, angularSizeRad, "Angular Size (rad)", "type=drag,minValue=0.00001,maxValue=0.02,speed=0.00001,format=%.6f")
		float angularSizeRad = 2.0e-4f;

		// Per-star blackbody chromaticity vs. one fixed tint, mirroring the
		// atmosphere's Use Temperature model.
		VEPROPERTY(SpaceStarsParams, bool, useTemperature, "Use Temperature", "type=bool")
		bool useTemperature = true;

		VEPROPERTY(SpaceStarsParams, glm::vec3, starColor, "Star Color", "type=drag")
		glm::vec3 starColor = glm::vec3(1.0f);

		VEPROPERTY(SpaceStarsParams, float, temperatureMinK, "Temperature Min (K)", "type=drag,minValue=1000,maxValue=40000,speed=10")
		float temperatureMinK = 3000.0f;

		VEPROPERTY(SpaceStarsParams, float, temperatureMaxK, "Temperature Max (K)", "type=drag,minValue=1000,maxValue=40000,speed=10")
		float temperatureMaxK = 25000.0f;

		void serialize(JsonWriter& w) const {
			w.set("layers", layers);
			w.set("density", density);
			w.set("brightness", brightness);
			w.set("angularSizeRad", angularSizeRad);
			w.set("useTemperature", useTemperature);
			w.set("starColor", starColor);
			w.set("temperatureMinK", temperatureMinK);
			w.set("temperatureMaxK", temperatureMaxK);
		}

		void deserialize(const JsonReader& r) {
			layers = r.getInt("layers", layers);
			density = r.getFloat("density", density);
			brightness = r.getFloat("brightness", brightness);
			angularSizeRad = r.getFloat("angularSizeRad", angularSizeRad);
			useTemperature = r.getBool("useTemperature", useTemperature);
			starColor = r.getVec3("starColor", starColor);
			temperatureMinK = r.getFloat("temperatureMinK", temperatureMinK);
			temperatureMaxK = r.getFloat("temperatureMaxK", temperatureMaxK);
		}
	};

	// Galaxy backdrop, after Xavier Benech's "Galaxy Trip" (CC BY-NC-SA 3.0):
	// a two-level domain warp over a marble field, then the reference's colour
	// shaping chain. Two things had to change for a static sky: the field is a
	// function of world direction instead of equirectangular uv (no pole
	// collapse, no seam at -pi), and there is no time term at all, so the sky is
	// fixed in world space and never drifts with the camera.
	// Banded emission nebula: a domain-warped fbm density, cut by dust lanes,
	// held to a band around the galactic plane and lit by a narrow palette (cold
	// blue body, rose cores, pale warm crests). Everything is a function of world
	// direction, so the sky is fixed in space.
	VESTRUCT(SpaceNebulaParams)
	struct SpaceNebulaParams {
		VEPROPERTY(SpaceNebulaParams, bool, enabled, "Enabled", "type=bool")
		bool enabled = true;

		VEPROPERTY(SpaceNebulaParams, float, brightness, "Brightness", "type=drag,minValue=0,maxValue=8,speed=0.01")
		float brightness = 2.0f;

		// Frequency of the gas field on the direction sphere. Low values give a
		// few huge sheets; high values break the band into many small clouds.
		VEPROPERTY(SpaceNebulaParams, float, scale, "Texture Scale", "type=drag,minValue=0.1,maxValue=16,speed=0.01")
		float scale = 1.8f;

		// How far the domain warp bends the field: 0 is smooth fog, around 1 the
		// crests stretch into filaments, past 2 the structure starts to smear.
		VEPROPERTY(SpaceNebulaParams, float, warp, "Domain Warp", "type=drag,minValue=0,maxValue=2,speed=0.01")
		float warp = 0.6f;

		VEPROPERTY(SpaceNebulaParams, glm::vec3, tint, "Tint", "type=drag")
		glm::vec3 tint = glm::vec3(1.0f);

		// The galactic plane. The nebula is masked by the angular distance from
		// this axis, so the field reads as one band across the sky rather than
		// fog everywhere. Band Width is the Gaussian's half-width in cos(a).
		VEPROPERTY(SpaceNebulaParams, glm::vec3, bandAxis, "Band Axis", "type=drag")
		glm::vec3 bandAxis = glm::vec3(0.0f, 1.0f, 0.0f);

		VEPROPERTY(SpaceNebulaParams, float, bandWidth, "Band Width", "type=drag,minValue=0.05,maxValue=1.5,speed=0.01")
		float bandWidth = 0.55f;

		void serialize(JsonWriter& w) const {
			w.set("enabled", enabled);
			w.set("brightness", brightness);
			w.set("scale", scale);
			w.set("warp", warp);
			w.set("tint", tint);
			w.set("bandAxis", bandAxis);
			w.set("bandWidth", bandWidth);
		}

		void deserialize(const JsonReader& r) {
			enabled = r.getBool("enabled", enabled);
			brightness = r.getFloat("brightness", brightness);
			scale = r.getFloat("scale", scale);
			warp = r.getFloat("warp", warp);
			tint = r.getVec3("tint", tint);
			bandAxis = r.getVec3("bandAxis", bandAxis);
			bandWidth = r.getFloat("bandWidth", bandWidth);
		}
	};

	VESTRUCT(SpaceBackgroundParams)
	struct SpaceBackgroundParams {
		VEPROPERTY(SpaceBackgroundParams, bool, enabled, "Enabled", "type=bool")
		bool enabled = true;

		VEPROPERTY(SpaceBackgroundParams, int, seed, "Seed", "type=drag,minValue=0,maxValue=100000,speed=1")
		int seed = 0;

		VEPROPERTY(SpaceBackgroundParams, SpaceNebulaParams, nebula, "nebula", "type=drag")
		SpaceNebulaParams nebula;

		VEPROPERTY(SpaceBackgroundParams, SpaceStarsParams, stars, "stars", "type=drag")
		SpaceStarsParams stars;

		void serialize(JsonWriter& w) const {
			w.set("enabled", enabled);
			w.set("seed", seed);
			w.beginObject("nebula");
			nebula.serialize(w);
			w.end();
			w.beginObject("stars");
			stars.serialize(w);
			w.end();
		}

		void deserialize(const JsonReader& r) {
			enabled = r.getBool("enabled", enabled);
			seed = r.getInt("seed", seed);
			nebula.deserialize(r.child("nebula"));
			stars.deserialize(r.child("stars"));
		}
	};

}

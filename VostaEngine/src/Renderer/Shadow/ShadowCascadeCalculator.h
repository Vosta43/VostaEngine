#pragma once

#include <glm.hpp>

namespace ve {

	// Three light view-projections for cascaded shadow maps, plus the receiver-side
	// data: which depth band each cascade covers and how big one of its texels is in
	// world units (the unit of the normal-offset bias).
	struct ShadowCascades {
		static constexpr int kCascades = 3;
		glm::mat4 lightVP[kCascades];
		float     splitFar[kCascades];    // camera-forward depth of each cascade's far edge
		float     texelWorld[kCascades];  // world size of one shadow-map texel in this cascade
		int       count = 0;
	};

	class ShadowCascadeCalculator {
	public:
		static constexpr int kCascades = ShadowCascades::kCascades;

		// Fits `count` cascades to the camera frustum out to `shadowDistance`, snapped to
		// shadow-map texels so the maps do not crawl as the camera moves.
		static ShadowCascades compute(const glm::mat4& view, const glm::mat4& projection,
		                              const glm::vec3& camPos, const glm::vec3& sunDir,
		                              float shadowDistance, const int mapRes[kCascades], int count);
	};

}

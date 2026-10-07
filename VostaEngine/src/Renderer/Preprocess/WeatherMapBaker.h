#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"

#include <vector>

namespace ve {

	// Bakes the 2D weather map the cloud shader samples for per-position coverage
	// (uniform u_WeatherMap). Mirrors Unity HDRP: R channel = coverage field, G/B
	// unused (R16F holds only the one channel). One-time startup cost like
	// WorleyNoiseBaker; the height gradient reads the sampled coverage to position
	// the cloud band, so coverage varies across the sky with a single per-step
	// 2D fetch instead of a shader-side noise field.
	class VE_API WeatherMapBaker {
	public:
		// R16F 2D texture, `resolution` x `resolution`, of a low-frequency Perlin
		// FBM sampled on the unit sphere — seamless in longitude, continuous at the
		// poles (no manual periodic wrapping). `cells` cells around the globe; the
		// `seed` is independent of the cloud noise bakes. R is remapped to [0.3, 1.0]
		// so the global slider dominates: effective coverage = R * u_CloudCoverage.
		static Ref<Texture2D> bake(uint32_t resolution, uint32_t cells, uint32_t seed);

		static std::vector<float> compute(uint32_t resolution, uint32_t cells, uint32_t seed);
		static Ref<Texture2D> build(uint32_t resolution, const std::vector<float>& data);
	};

}

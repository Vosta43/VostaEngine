#include "vepch.h"
#include "Noise/NoiseBaker.h"

#include "Noise/NoiseNodes.h"
#include "Noise/NoisePlan.h"
#include "Core/JobSystem.h"

#include <algorithm>

namespace ve {

	TextureResource bakeNoiseGraphTexture(const NoiseGraph& graph, uint32_t size) {
		TextureResource tex;
		tex.width = size;
		tex.height = size;
		tex.format = TextureFormat::RGBA;
		tex.pixels.resize((size_t)size * size * 4);

		if (size == 0)
			return tex;

		// The Output node owns the field's display range; without one, fall back to
		// the unit's native [-1, 1].
		float lo = -1.0f, hi = 1.0f;
		for (const auto& node : graph.nodes) {
			if (auto* out = dynamic_cast<NoiseOutputNode*>(node.get())) {
				lo = out->outputMin;
				hi = out->outputMax;
				break;
			}
		}
		const float range = hi - lo;

		const float half = (float)size * 0.5f;

		// One plan for the whole bake, then rows in parallel: the flattened evaluator
		// touches each node once per sample with no graph walking.
		const NoisePlan plan = buildNoisePlan(graph);
		JobSystem::get().parallelFor(0, (int)size, [&](int y) {
			thread_local NoisePlanScratch scratch;
			for (uint32_t x = 0; x < size; ++x) {
				const float v = plan.evaluate(scratch, (float)x - half, (float)y - half);
				const float t = range > 1e-6f ? (v - lo) / range : 0.5f;
				const uint8_t g = (uint8_t)std::clamp(t * 255.0f + 0.5f, 0.0f, 255.0f);

				uint8_t* px = &tex.pixels[((size_t)y * size + x) * 4];
				px[0] = px[1] = px[2] = g;
				px[3] = 255;
			}
		}).get();

		return tex;
	}

}

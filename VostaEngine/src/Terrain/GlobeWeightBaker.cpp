#include "vepch.h"
#include "Terrain/GlobeWeightBaker.h"
#include "Terrain/RulePlan.h"
#include "Core/JobSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ve {

	Ref<Texture2DArray> bakeGlobeWeights(const RuleGraph& graph, int layerCount,
	                                     int width, int height, float fieldExtent) {

		if (layerCount <= 1 || width <= 0 || height <= 0)
			return nullptr;

		RulePlan plan = buildRulePlan(graph);
		if (!plan.valid())
			return nullptr;

		std::vector<uint8_t> data((size_t)layerCount * width * height, 0);

		// Texel (u, v) is the equirectangular coordinate the shader recovers from a
		// fragment's direction; the rule graph is read at fieldExtent * (u, v), the
		// same 2D field the chart terrain samples. The baker never needs the direction
		// itself -- only the shader's veEquirect turns one into this (u, v).
		const auto bakeRow = [&](int y) {
			thread_local RulePlanScratch scratch;
			RuleEvalContext ctx;

			std::vector<float> w((size_t)(layerCount - 1), 0.0f);

			const float v = ((float)y + 0.5f) / (float)height;

			for (int x = 0; x < width; ++x) {
				const float u = ((float)x + 0.5f) / (float)width;
				ctx.sample = glm::vec2(u, v) * fieldExtent;

				plan.evaluate(scratch, ctx, w.data(), layerCount - 1);
				for (int i = 1; i < layerCount; ++i)
					data[(size_t)i * width * height + (size_t)y * width + x] =
						(uint8_t)std::lround(std::clamp(w[i - 1], 0.0f, 1.0f) * 255.0f);
			}
		};

		if (JobSystem::get().threadCount() > 1)
			JobSystem::get().parallelFor(0, height, bakeRow).get();
		else
			for (int y = 0; y < height; ++y)
				bakeRow(y);

		auto tex = Texture2DArray::create((uint32_t)width, (uint32_t)height, (uint32_t)layerCount,
		                                  TextureFormat::R8, nullptr);
		if (!tex)
			return nullptr;
		tex->setData(data.data(), (uint32_t)data.size());
		return tex;
	}

}

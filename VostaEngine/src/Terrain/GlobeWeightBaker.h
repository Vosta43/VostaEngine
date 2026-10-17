#pragma once

#include "Core/Core.h"
#include "Terrain/RuleGraph.h"
#include "Renderer/Texture.h"

namespace ve {

	// Globe weight atlas: equirectangular, u = longitude (width), v = latitude (height).
	inline constexpr int   kGlobeWeightWidth    = 2048;
	inline constexpr int   kGlobeWeightHeight   = 1024;

	// Bakes the rule graph over the sphere's equirectangular [0,1]^2 into one R8 slice
	// per layer (slice 0 unused). Texel (u, v) is the graph sampled at fieldExtent *
	// (u, v) -- the same 2D field the chart terrain reads. Returns null when the graph
	// has no terminal. `graph` must outlive the call; render thread only.
	VE_API Ref<Texture2DArray> bakeGlobeWeights(const RuleGraph& graph, int layerCount,
	                                            int width, int height, float fieldExtent);

}

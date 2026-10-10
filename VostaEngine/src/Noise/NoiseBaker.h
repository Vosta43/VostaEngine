#pragma once

#include "Core/Core.h"
#include "Asset/TextureResource.h"
#include "Noise/NoiseGraph.h"

#include <cstdint>

namespace ve {

	// Bakes a noise graph into a square grayscale RGBA texture resource. Each texel
	// samples the graph at (x - size/2, y - size/2) -- a centred window -- and is
	// written through the Output node's range as an opaque grey. CPU evaluation,
	// rows in parallel over the JobSystem.
	//
	// The one implementation shared by the editor's live preview / export and the
	// MCP bake command, so a graph can never bake two different ways. The result is
	// ready for TextureImporter::serialize (or a GPU upload).
	VE_API TextureResource bakeNoiseGraphTexture(const NoiseGraph& graph, uint32_t size);

}

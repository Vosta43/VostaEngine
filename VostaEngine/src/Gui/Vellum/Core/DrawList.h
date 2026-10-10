#pragma once

#include "Layout.h"

#include <cstdint>
#include <vector>

namespace ve::vellum {

// Opaque GPU texture handle. 0 is reserved for the built-in atlas (a 1x1 white
// texture for now; the glyph atlas later). The core never interprets it.
using TextureId = uint64_t;

// One UI vertex. Every widget tessellates to textured quads; a solid fill samples
// the atlas white pixel and multiplies it by `color`.
struct Vertex {
	glm::vec2 pos;
	glm::vec2 uv;
	glm::vec4 color;
};

// One batched draw call: a contiguous range in the shared index buffer plus the
// GPU state it needs. Adjacent geometry merges into one command while
// (clip, texture) stay equal.
struct DrawCommand {
	uint32_t  firstIndex = 0;
	uint32_t  indexCount = 0;
	Rect      clip{};
	TextureId texture = 0;
};

// A frame's geometry in two shared buffers, sliced into draw commands.
struct DrawList {
	std::vector<Vertex>      vertices;
	std::vector<uint32_t>    indices;
	std::vector<DrawCommand> commands;

	void clear() { vertices.clear(); indices.clear(); commands.clear(); }
};

}

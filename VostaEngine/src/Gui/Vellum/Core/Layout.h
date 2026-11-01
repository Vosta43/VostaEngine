#pragma once

#include <glm.hpp>
#include <algorithm>

namespace ve::vellum {

// Axis-aligned rectangle: position + size.
struct Rect {
	glm::vec2 pos{ 0.0f };
	glm::vec2 size{ 0.0f };
};

// The overlapping part of two rects; zero-sized when they do not overlap.
inline Rect intersect(const Rect& a, const Rect& b) {
	const float x0 = std::max(a.pos.x, b.pos.x);
	const float y0 = std::max(a.pos.y, b.pos.y);
	const float x1 = std::min(a.pos.x + a.size.x, b.pos.x + b.size.x);
	const float y1 = std::min(a.pos.y + a.size.y, b.pos.y + b.size.y);
	return { { x0, y0 }, { std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0) } };
}

inline bool operator==(const Rect& a, const Rect& b) {
	return a.pos == b.pos && a.size == b.size;
}

// Point-in-rect hit test. Half-open: the max edges are excluded so adjacent
// rects never both claim the same pixel.
inline bool contains(const Rect& r, const glm::vec2& p) {
	return p.x >= r.pos.x && p.x < r.pos.x + r.size.x &&
	       p.y >= r.pos.y && p.y < r.pos.y + r.size.y;
}

// Maps the logical (design-pixel) space widgets lay out in onto the surface's
// physical pixels. The tree arranges and paints in logical units; the renderer
// applies the scale and the router applies its inverse to the mouse, so a design
// laid out once survives any window size without a rebuild.
struct LayoutTransform {
	float     scale = 1.0f;      // logical unit -> surface pixels
	glm::vec2 origin{ 0.0f };    // logical (0,0) in surface pixels

	glm::vec2 toLogical(const glm::vec2& p) const { return (p - origin) / scale; }
	glm::vec2 toSurface(const glm::vec2& p) const { return p * scale + origin; }

	// What a surface of `surfaceSize` shows in logical units — the design rect,
	// letterbox bars excluded.
	glm::vec2 logicalSize(const glm::vec2& surfaceSize) const {
		return scale > 0.0f ? (surfaceSize - origin * 2.0f) / scale : surfaceSize;
	}
};

// Uniform scale that shows all of `design` centred in `surface`: the fit the
// legacy UI ortho produced (letterboxing the surplus axis, never distorting).
inline LayoutTransform fitLayout(const glm::vec2& surface, const glm::vec2& design) {
	if (design.x <= 0.0f || design.y <= 0.0f || surface.x <= 0.0f || surface.y <= 0.0f)
		return {};
	const float scale = std::min(surface.x / design.x, surface.y / design.y);
	return { scale, (surface - design * scale) * 0.5f };
}

}

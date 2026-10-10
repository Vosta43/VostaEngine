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

}

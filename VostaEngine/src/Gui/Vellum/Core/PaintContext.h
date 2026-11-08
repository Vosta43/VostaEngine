#pragma once

#include "Core/Core.h"
#include "DrawList.h"
#include "Layout.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ve::vellum {

// Widget-facing paint API. Tessellates immediately into a DrawList and batches:
// adjacent fills merge into one DrawCommand while (clip, texture) stay equal.
class VE_API PaintContext {
public:
	// Start a frame: clears the list and seeds the clip stack with the surface rect.
	void beginFrame(const glm::vec2& surfaceSize);

	void fillRect(const Rect& rect, const glm::vec4& color);

	// A textured quad. UVs are top-left / bottom-right corners of the sample region.
	void drawQuad(const Rect& rect, const glm::vec2& uv0, const glm::vec2& uv1,
		TextureId texture, const glm::vec4& color);

	// Same quad with one colour per corner, in top-left, top-right, bottom-right,
	// bottom-left order. The rasteriser interpolates across the quad, so a single
	// glyph can carry a vertical or horizontal gradient.
	void drawQuad(const Rect& rect, const glm::vec2& uv0, const glm::vec2& uv1,
		TextureId texture, const glm::vec4& topLeft, const glm::vec4& topRight,
		const glm::vec4& bottomRight, const glm::vec4& bottomLeft);

	void pushClip(const Rect& rect);
	void popClip();

	// 2D affine applied to everything painted until the matching pop. Widgets
	// animate through here, never through their layout rect. `pivot` is in the
	// current space (the widget's rect position plus its normalised pivot).
	void pushTransform(const glm::vec2& translation, float rotationRad,
	                   const glm::vec2& scale, const glm::vec2& pivot);
	void popTransform();

	// Multiplies into the running alpha of every vertex drawn until the pop.
	void pushOpacity(float opacity);
	void popOpacity();

	DrawList&       list()       { return m_list; }
	const DrawList& list() const { return m_list; }

private:
	void addCommand(uint32_t firstIndex, TextureId texture);
	Rect currentClip() const;
	glm::vec2 toScreen(const glm::vec2& p) const;

	DrawList          m_list;
	std::vector<Rect> m_clips;  // back() is the resolved current clip

	glm::mat3               m_xform{ 1.0f };  // local -> surface logical space
	std::vector<glm::mat3>  m_xforms;
	float                   m_opacity = 1.0f;
	std::vector<float>      m_opacities;
};

inline void PaintContext::beginFrame(const glm::vec2& surfaceSize) {
	m_list.clear();
	m_clips.clear();
	m_clips.push_back({ { 0.0f, 0.0f }, surfaceSize });
	m_xform = glm::mat3(1.0f);
	m_xforms.clear();
	m_opacity = 1.0f;
	m_opacities.clear();
}

inline void PaintContext::fillRect(const Rect& rect, const glm::vec4& color) {
	// Sample the centre of the 1x1 white texture in the built-in atlas.
	const glm::vec2 uv{ 0.5f, 0.5f };
	drawQuad(rect, uv, uv, 0, color);
}

inline void PaintContext::drawQuad(const Rect& rect, const glm::vec2& uv0, const glm::vec2& uv1,
	TextureId texture, const glm::vec4& color) {
	drawQuad(rect, uv0, uv1, texture, color, color, color, color);
}

inline void PaintContext::drawQuad(const Rect& rect, const glm::vec2& uv0, const glm::vec2& uv1,
	TextureId texture, const glm::vec4& topLeft, const glm::vec4& topRight,
	const glm::vec4& bottomRight, const glm::vec4& bottomLeft) {
	const uint32_t first = static_cast<uint32_t>(m_list.indices.size());
	const uint32_t base  = static_cast<uint32_t>(m_list.vertices.size());

	const glm::vec2 p0 = toScreen({ rect.pos.x,               rect.pos.y               });
	const glm::vec2 p1 = toScreen({ rect.pos.x + rect.size.x, rect.pos.y               });
	const glm::vec2 p2 = toScreen({ rect.pos.x + rect.size.x, rect.pos.y + rect.size.y });
	const glm::vec2 p3 = toScreen({ rect.pos.x,               rect.pos.y + rect.size.y });

	// Vertex colours are baked here, so batches merge on (clip, texture) alone.
	const auto shade = [this](glm::vec4 c) { c.a *= m_opacity; return c; };

	m_list.vertices.push_back({ p0, { uv0.x, uv0.y }, shade(topLeft) });
	m_list.vertices.push_back({ p1, { uv1.x, uv0.y }, shade(topRight) });
	m_list.vertices.push_back({ p2, { uv1.x, uv1.y }, shade(bottomRight) });
	m_list.vertices.push_back({ p3, { uv0.x, uv1.y }, shade(bottomLeft) });

	m_list.indices.push_back(base + 0);
	m_list.indices.push_back(base + 1);
	m_list.indices.push_back(base + 2);
	m_list.indices.push_back(base + 2);
	m_list.indices.push_back(base + 3);
	m_list.indices.push_back(base + 0);

	addCommand(first, texture);
}

inline void PaintContext::pushClip(const Rect& rect) {
	// The scissor is axis-aligned, so a transformed clip collapses to its bounds.
	// Exact under translation and scale; conservative under rotation.
	const glm::vec2 a = toScreen(rect.pos);
	const glm::vec2 b = toScreen({ rect.pos.x + rect.size.x, rect.pos.y });
	const glm::vec2 c = toScreen({ rect.pos.x + rect.size.x, rect.pos.y + rect.size.y });
	const glm::vec2 d = toScreen({ rect.pos.x, rect.pos.y + rect.size.y });

	const float x0 = std::min(std::min(a.x, b.x), std::min(c.x, d.x));
	const float y0 = std::min(std::min(a.y, b.y), std::min(c.y, d.y));
	const float x1 = std::max(std::max(a.x, b.x), std::max(c.x, d.x));
	const float y1 = std::max(std::max(a.y, b.y), std::max(c.y, d.y));

	const Rect bounds{ { x0, y0 }, { std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0) } };
	m_clips.push_back(intersect(currentClip(), bounds));
}

inline void PaintContext::popClip() {
	VE_CORE_ASSERT(m_clips.size() > 1, "popClip without a matching pushClip");
	if (m_clips.size() > 1)
		m_clips.pop_back();
}

inline void PaintContext::addCommand(uint32_t firstIndex, TextureId texture) {
	const Rect     clip  = currentClip();
	const uint32_t total = static_cast<uint32_t>(m_list.indices.size());

	// Indices are appended in order, so a merge just grows the last command's count.
	if (!m_list.commands.empty()) {
		DrawCommand& last = m_list.commands.back();
		if (last.texture == texture && last.clip == clip) {
			last.indexCount = total - last.firstIndex;
			return;
		}
	}
	m_list.commands.push_back({ firstIndex, total - firstIndex, clip, texture });
}

inline Rect PaintContext::currentClip() const {
	return m_clips.empty() ? Rect{} : m_clips.back();
}

inline glm::vec2 PaintContext::toScreen(const glm::vec2& p) const {
	const glm::vec3 q = m_xform * glm::vec3(p.x, p.y, 1.0f);
	return { q.x, q.y };
}

inline void PaintContext::pushTransform(const glm::vec2& translation, float rotationRad,
                                        const glm::vec2& scale, const glm::vec2& pivot) {
	// p -> R * S * (p - pivot) + pivot + translation
	const float c = std::cos(rotationRad);
	const float s = std::sin(rotationRad);

	glm::mat3 local(1.0f);
	local[0] = glm::vec3(c * scale.x, s * scale.x, 0.0f);
	local[1] = glm::vec3(-s * scale.y, c * scale.y, 0.0f);

	const glm::vec2 scaledPivot{ pivot.x * scale.x, pivot.y * scale.y };
	const glm::vec2 rotated{ c * scaledPivot.x - s * scaledPivot.y,
	                         s * scaledPivot.x + c * scaledPivot.y };
	local[2] = glm::vec3(pivot.x + translation.x - rotated.x,
	                     pivot.y + translation.y - rotated.y, 1.0f);

	m_xforms.push_back(m_xform);
	m_xform = m_xform * local;
}

inline void PaintContext::popTransform() {
	VE_CORE_ASSERT(!m_xforms.empty(), "popTransform without a matching pushTransform");
	if (m_xforms.empty())
		return;
	m_xform = m_xforms.back();
	m_xforms.pop_back();
}

inline void PaintContext::pushOpacity(float opacity) {
	m_opacities.push_back(m_opacity);
	m_opacity *= std::clamp(opacity, 0.0f, 1.0f);
}

inline void PaintContext::popOpacity() {
	VE_CORE_ASSERT(!m_opacities.empty(), "popOpacity without a matching pushOpacity");
	if (m_opacities.empty())
		return;
	m_opacity = m_opacities.back();
	m_opacities.pop_back();
}

}

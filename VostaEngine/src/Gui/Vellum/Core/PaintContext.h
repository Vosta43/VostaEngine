#pragma once

#include "Core/Core.h"
#include "DrawList.h"
#include "Layout.h"

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

	void pushClip(const Rect& rect);
	void popClip();

	DrawList&       list()       { return m_list; }
	const DrawList& list() const { return m_list; }

private:
	void addCommand(uint32_t firstIndex, TextureId texture);
	Rect currentClip() const;

	DrawList          m_list;
	std::vector<Rect> m_clips;  // back() is the resolved current clip
};

inline void PaintContext::beginFrame(const glm::vec2& surfaceSize) {
	m_list.clear();
	m_clips.clear();
	m_clips.push_back({ { 0.0f, 0.0f }, surfaceSize });
}

inline void PaintContext::fillRect(const Rect& rect, const glm::vec4& color) {
	// Sample the centre of the 1x1 white texture in the built-in atlas.
	const glm::vec2 uv{ 0.5f, 0.5f };
	drawQuad(rect, uv, uv, 0, color);
}

inline void PaintContext::drawQuad(const Rect& rect, const glm::vec2& uv0, const glm::vec2& uv1,
	TextureId texture, const glm::vec4& color) {
	const uint32_t first = static_cast<uint32_t>(m_list.indices.size());
	const uint32_t base  = static_cast<uint32_t>(m_list.vertices.size());

	m_list.vertices.push_back({ { rect.pos.x,               rect.pos.y               }, { uv0.x, uv0.y }, color });
	m_list.vertices.push_back({ { rect.pos.x + rect.size.x, rect.pos.y               }, { uv1.x, uv0.y }, color });
	m_list.vertices.push_back({ { rect.pos.x + rect.size.x, rect.pos.y + rect.size.y }, { uv1.x, uv1.y }, color });
	m_list.vertices.push_back({ { rect.pos.x,               rect.pos.y + rect.size.y }, { uv0.x, uv1.y }, color });

	m_list.indices.push_back(base + 0);
	m_list.indices.push_back(base + 1);
	m_list.indices.push_back(base + 2);
	m_list.indices.push_back(base + 2);
	m_list.indices.push_back(base + 3);
	m_list.indices.push_back(base + 0);

	addCommand(first, texture);
}

inline void PaintContext::pushClip(const Rect& rect) {
	m_clips.push_back(intersect(currentClip(), rect));
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

}

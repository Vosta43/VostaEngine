#include "vepch.h"
#include "GlyphAtlas.h"

#include "Core/Log.h"
#include "Gui/Vellum/VellumRenderer.h"
#include "Renderer/Texture.h"

#include <algorithm>

namespace ve::vellum {

	namespace {
		// 1024 wide so CJK squares pack several per shelf row; the height grows by
		// doubling and only it ever changes, which keeps cached U coordinates valid.
		constexpr uint32_t kAtlasWidth     = 1024;
		constexpr uint32_t kAtlasHeight    = 256;
		constexpr uint32_t kPad            = 1;     // gutter between glyphs
		constexpr uint32_t kMaxAtlasHeight = 8192;  // safety net against runaway growth
	}

	std::size_t GlyphAtlas::KeyHash::operator()(const Key& key) const {
		std::size_t h = std::hash<uint64_t>()(key.fontId);
		const auto mix = [&h](std::size_t v) {
			h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
		};
		mix(std::hash<uint32_t>()(key.codepoint));
		mix(std::hash<float>()(key.size));
		return h;
	}

	GlyphAtlas::GlyphAtlas(VellumRenderer& renderer)
		: m_renderer(&renderer)
		, m_id(renderer.acquireTextureId()) {
		m_width = kAtlasWidth;
		m_height = kAtlasHeight;
		m_cursorX = kPad;
		m_cursorY = kPad;
		m_pixels.assign(static_cast<std::size_t>(m_width) * m_height * 4, 0);
		m_texture = Texture2D::create(m_width, m_height, TextureFormat::RGBA);
		if (!m_texture) {
			VE_CORE_ERROR_PRINT("Vellum: glyph atlas texture creation failed");
			return;
		}
		m_texture->setData(m_pixels.data(), static_cast<uint32_t>(m_pixels.size()));
		m_renderer->registerTexture(m_id, m_texture);
	}

	const GlyphAtlas::Glyph* GlyphAtlas::find(const Key& key) const {
		const auto found = m_glyphs.find(key);
		return found != m_glyphs.end() ? &found->second : nullptr;
	}

	const GlyphAtlas::Glyph* GlyphAtlas::insert(const Key& key, uint32_t w, uint32_t h,
	                                            const unsigned char* coverage,
	                                            glm::vec2 bearing, float advance) {
		if (const Glyph* existing = find(key))
			return existing;

		Glyph glyph;
		glyph.bearing = bearing;
		glyph.advance = advance;

		if (coverage && w > 0 && h > 0) {
			if (place(w, h, coverage, glyph.uv0, glyph.uv1)) {
				glyph.size = { static_cast<float>(w), static_cast<float>(h) };
			}
			else {
				// Wider than the atlas, or growth capped: keep the advance, draw
				// nothing. Layout stays correct for the codepoint either way.
				glyph.uv0 = glyph.uv1 = glm::vec2(0.0f);
				glyph.size = glm::vec2(0.0f);
			}
		}

		return &m_glyphs.emplace(key, glyph).first->second;
	}

	bool GlyphAtlas::place(uint32_t w, uint32_t h, const unsigned char* coverage,
	                       glm::vec2& uv0, glm::vec2& uv1) {
		if (w + 2 * kPad > m_width)
			return false;

		// Shelf packing: left-to-right, wrap when the slot would cross the right
		// edge, grow when the new row would fall off the bottom.
		if (m_cursorX != kPad && m_cursorX + w + kPad > m_width) {
			m_cursorX = kPad;
			m_cursorY += m_rowHeight + kPad;
			m_rowHeight = 0;
		}
		if (m_cursorY + h + kPad > m_height)
			grow(m_cursorY + h + kPad);
		if (m_cursorY + h + kPad > m_height)
			return false;   // growth capped by kMaxAtlasHeight

		const uint32_t x = m_cursorX;
		const uint32_t y = m_cursorY;
		m_cursorX += w + kPad;
		m_rowHeight = std::max(m_rowHeight, h);

		// Build the tightly-packed block for the sub-upload while writing the same
		// pixels into the slab a later grow re-uploads from. RGB is white so the UI
		// shader's `v_Color * texture(...)` tints text with no special case.
		std::vector<unsigned char> block(static_cast<std::size_t>(w) * h * 4);
		for (uint32_t row = 0; row < h; ++row) {
			unsigned char* slabRow  = &m_pixels[(static_cast<std::size_t>(y + row) * m_width + x) * 4];
			unsigned char* blockRow = &block[static_cast<std::size_t>(row) * w * 4];
			const unsigned char* src = coverage + static_cast<std::size_t>(row) * w;
			for (uint32_t col = 0; col < w; ++col) {
				const unsigned char alpha = src[col];
				const std::size_t o = static_cast<std::size_t>(col) * 4;
				slabRow[o + 0] = 255; slabRow[o + 1] = 255; slabRow[o + 2] = 255; slabRow[o + 3] = alpha;
				blockRow[o + 0] = 255; blockRow[o + 1] = 255; blockRow[o + 2] = 255; blockRow[o + 3] = alpha;
			}
		}

		m_texture->setSubData(block.data(), x, y, w, h);

		uv0 = { static_cast<float>(x) / m_width, static_cast<float>(y) / m_height };
		uv1 = { static_cast<float>(x + w) / m_width, static_cast<float>(y + h) / m_height };
		return true;
	}

	void GlyphAtlas::grow(uint32_t requiredHeight) {
		uint32_t newHeight = m_height;
		while (newHeight < requiredHeight && newHeight < kMaxAtlasHeight)
			newHeight *= 2;
		if (newHeight == m_height)
			return;

		// Width is unchanged, so the old rows keep their layout: copy them into the
		// head of a taller slab. Only swap state once the new texture is real, so a
		// failed growth leaves the atlas consistent.
		Ref<Texture2D> texture = Texture2D::create(m_width, newHeight, TextureFormat::RGBA);
		if (!texture) {
			VE_CORE_ERROR_PRINT("Vellum: glyph atlas growth failed");
			return;
		}

		std::vector<unsigned char> taller(static_cast<std::size_t>(m_width) * newHeight * 4, 0);
		std::copy(m_pixels.begin(), m_pixels.end(), taller.begin());
		texture->setData(taller.data(), static_cast<uint32_t>(taller.size()));

		// Every cached V coordinate was normalised by the old height; U is safe
		// because the width never changes.
		const float vScale = static_cast<float>(m_height) / static_cast<float>(newHeight);
		for (auto& [key, glyph] : m_glyphs) {
			glyph.uv0.y *= vScale;
			glyph.uv1.y *= vScale;
		}

		m_pixels.swap(taller);
		m_height = newHeight;
		m_texture = std::move(texture);
		m_renderer->registerTexture(m_id, m_texture);
	}

}

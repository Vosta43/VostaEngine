#include "vepch.h"
#include "FontAtlas.h"

#include "Utf8.h"
#include "Core/Log.h"
#include "Gui/Vellum/VellumRenderer.h"
#include "Renderer/Texture.h"

#include "stb_truetype.h"

#include <algorithm>
#include <fstream>

namespace ve::vellum {

	namespace {
		constexpr uint32_t kAtlasWidth    = 512;
		constexpr uint32_t kAtlasHeight   = 128;   // grows by doubling
		constexpr uint32_t kPad           = 1;     // gutter between glyphs
		constexpr uint32_t kMaxAtlasHeight = 8192; // safety net against runaway growth
	}

	FontAtlas::~FontAtlas() {
		delete m_info;
	}

	Ref<FontAtlas> FontAtlas::create(const std::string& ttfPath, float pixelSize,
	                                 TextureId id, VellumRenderer& renderer) {
		std::ifstream file(ttfPath, std::ios::binary | std::ios::ate);
		if (!file) {
			VE_CORE_ERROR_PRINT("Vellum: font not found: %s", ttfPath.c_str());
			return nullptr;
		}
		const std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<unsigned char> ttf(static_cast<std::size_t>(size));
		if (!file.read(reinterpret_cast<char*>(ttf.data()), size)) {
			VE_CORE_ERROR_PRINT("Vellum: failed to read font: %s", ttfPath.c_str());
			return nullptr;
		}

		auto atlas = CreateRef<FontAtlas>();
		atlas->m_renderer = &renderer;
		atlas->m_id = id;
		atlas->m_ttf = std::move(ttf);

		// stb keeps a pointer into m_ttf; the buffer therefore outlives the atlas.
		atlas->m_info = new stbtt_fontinfo();
		if (!stbtt_InitFont(atlas->m_info, atlas->m_ttf.data(), 0)) {
			VE_CORE_ERROR_PRINT("Vellum: not a valid TrueType font: %s", ttfPath.c_str());
			return nullptr;
		}

		atlas->m_scale = stbtt_ScaleForPixelHeight(atlas->m_info, pixelSize);
		int ascent = 0, descent = 0, lineGap = 0;
		stbtt_GetFontVMetrics(atlas->m_info, &ascent, &descent, &lineGap);
		atlas->m_ascent = ascent * atlas->m_scale;
		atlas->m_lineHeight = (ascent - descent + lineGap) * atlas->m_scale;

		// Empty texture now; glyphs stream into it as they are first requested.
		atlas->m_width = kAtlasWidth;
		atlas->m_height = kAtlasHeight;
		atlas->m_cursorX = kPad;
		atlas->m_cursorY = kPad;
		atlas->m_pixels.assign(static_cast<std::size_t>(kAtlasWidth) * kAtlasHeight * 4, 0);
		atlas->m_texture = Texture2D::create(kAtlasWidth, kAtlasHeight, TextureFormat::RGBA);
		if (!atlas->m_texture) {
			VE_CORE_ERROR_PRINT("Vellum: font atlas texture creation failed");
			return nullptr;
		}
		atlas->m_texture->setData(atlas->m_pixels.data(), static_cast<uint32_t>(atlas->m_pixels.size()));
		renderer.registerTexture(id, atlas->m_texture);

		return atlas;
	}

	const FontAtlas::Glyph* FontAtlas::get(uint32_t codepoint) {
		const auto found = m_glyphs.find(codepoint);
		if (found != m_glyphs.end())
			return &found->second;

		const int cp = static_cast<int>(codepoint);
		int advance = 0, lsb = 0;
		stbtt_GetCodepointHMetrics(m_info, cp, &advance, &lsb);

		int w = 0, h = 0, xoff = 0, yoff = 0;
		unsigned char* bitmap = stbtt_GetCodepointBitmap(m_info, m_scale, m_scale, cp, &w, &h, &xoff, &yoff);

		Glyph glyph;
		glyph.advance = advance * m_scale;
		glyph.size = { static_cast<float>(w), static_cast<float>(h) };
		glyph.bearing = { static_cast<float>(xoff), static_cast<float>(yoff) };

		if (bitmap && w > 0 && h > 0) {
			if (!insert(static_cast<uint32_t>(w), static_cast<uint32_t>(h), bitmap, glyph.uv0, glyph.uv1)) {
				// Wider than the atlas: keep the advance, draw nothing.
				glyph.uv0 = glyph.uv1 = glm::vec2(0.0f);
				glyph.size = glm::vec2(0.0f);
			}
		}
		stbtt_FreeBitmap(bitmap, nullptr);

		return &m_glyphs.emplace(codepoint, glyph).first->second;
	}

	bool FontAtlas::insert(uint32_t w, uint32_t h, const unsigned char* bitmap,
	                       glm::vec2& uv0, glm::vec2& uv1) {
		if (w + 2 * kPad > m_width)
			return false;

		// Shelf packing: place left-to-right, wrap when the slot would cross the
		// right edge, grow when the new row would fall off the bottom.
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
		// pixels into the slab that a later grow will re-upload from.
		std::vector<unsigned char> block(static_cast<std::size_t>(w) * h * 4);
		for (uint32_t row = 0; row < h; ++row) {
			unsigned char* slabRow  = &m_pixels[(static_cast<std::size_t>(y + row) * m_width + x) * 4];
			unsigned char* blockRow = &block[static_cast<std::size_t>(row) * w * 4];
			const unsigned char* src = bitmap + static_cast<std::size_t>(row) * w;
			for (uint32_t col = 0; col < w; ++col) {
				const unsigned char coverage = src[col];
				const std::size_t o = static_cast<std::size_t>(col) * 4;
				slabRow[o + 0] = 255; slabRow[o + 1] = 255; slabRow[o + 2] = 255; slabRow[o + 3] = coverage;
				blockRow[o + 0] = 255; blockRow[o + 1] = 255; blockRow[o + 2] = 255; blockRow[o + 3] = coverage;
			}
		}

		m_texture->setSubData(block.data(), x, y, w, h);

		uv0 = { static_cast<float>(x) / m_width, static_cast<float>(y) / m_height };
		uv1 = { static_cast<float>(x + w) / m_width, static_cast<float>(y + h) / m_height };
		return true;
	}

	void FontAtlas::grow(uint32_t requiredHeight) {
		uint32_t newHeight = m_height;
		while (newHeight < requiredHeight && newHeight < kMaxAtlasHeight)
			newHeight *= 2;
		if (newHeight == m_height)
			return;

		// Width is unchanged, so the old rows keep their layout: copy them into the
		// head of a taller slab, then re-create the texture and re-register it.
		std::vector<unsigned char> taller(static_cast<std::size_t>(m_width) * newHeight * 4, 0);
		std::copy(m_pixels.begin(), m_pixels.end(), taller.begin());
		m_pixels.swap(taller);

		// Every cached glyph's V coordinate was normalised by the old height; the
		// taller texture shifts them. U is safe — the width never changes.
		const float vScale = static_cast<float>(m_height) / static_cast<float>(newHeight);
		for (auto& [codepoint, glyph] : m_glyphs) {
			glyph.uv0.y *= vScale;
			glyph.uv1.y *= vScale;
		}

		m_height = newHeight;
		m_texture = Texture2D::create(m_width, m_height, TextureFormat::RGBA);
		if (!m_texture) {
			VE_CORE_ERROR_PRINT("Vellum: font atlas growth failed");
			return;
		}
		m_texture->setData(m_pixels.data(), static_cast<uint32_t>(m_pixels.size()));
		m_renderer->registerTexture(m_id, m_texture);
	}

	glm::vec2 FontAtlas::measure(const std::string& text) {
		float width = 0.0f;
		std::size_t i = 0;
		while (i < text.size()) {
			const uint32_t cp = utf8Next(text, i);
			if (cp == 0)
				continue;
			if (const Glyph* g = get(cp))
				width += g->advance;
		}
		return { width, m_lineHeight };
	}

}

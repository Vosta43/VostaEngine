#include "vepch.h"
#include "FontAtlas.h"

#include "Font.h"
#include "Utf8.h"
#include "Gui/Vellum/VellumRenderer.h"

#include "stb_truetype.h"

namespace ve::vellum {

	FontAtlas::FontAtlas(Font& font, float pixelSize)
		: m_font(&font)
		, m_pixelSize(pixelSize)
		, m_renderer(font.m_renderer) {
		const auto& faces = font.faceData();
		m_scales.reserve(faces.size());
		for (const auto& face : faces)
			m_scales.push_back(stbtt_ScaleForPixelHeight(face->info, pixelSize));

		// Line metrics come from the first face so every script in a mixed line
		// shares one baseline and line box.
		int ascent = 0, descent = 0, lineGap = 0;
		stbtt_GetFontVMetrics(faces.front()->info, &ascent, &descent, &lineGap);
		const float scale = m_scales.front();
		m_ascent = ascent * scale;
		m_lineHeight = (ascent - descent + lineGap) * scale;
	}

	TextureId FontAtlas::textureId() const {
		return m_renderer->glyphAtlas().textureId();
	}

	const FontAtlas::Glyph* FontAtlas::get(uint32_t codepoint) {
		GlyphAtlas& atlas = m_renderer->glyphAtlas();

		const GlyphAtlas::Key key{ m_font->identity(), m_pixelSize, codepoint };
		if (const Glyph* cached = atlas.find(key))
			return cached;

		const auto& faces = m_font->faceData();
		const int cp = static_cast<int>(codepoint);

		// First face in the chain with an outline for this codepoint serves it; when
		// every face misses, the primary face renders its .notdef exactly as before.
		const FaceData* face = faces.front().get();
		float scale = m_scales.front();
		for (std::size_t i = 0; i < faces.size(); ++i) {
			if (stbtt_FindGlyphIndex(faces[i]->info, cp) != 0) {
				face = faces[i].get();
				scale = m_scales[i];
				break;
			}
		}

		int advance = 0, lsb = 0;
		stbtt_GetCodepointHMetrics(face->info, cp, &advance, &lsb);

		int w = 0, h = 0, xoff = 0, yoff = 0;
		unsigned char* bitmap = stbtt_GetCodepointBitmap(
			face->info, scale, scale, cp, &w, &h, &xoff, &yoff);

		const Glyph* glyph = atlas.insert(
			key,
			bitmap && w > 0 ? static_cast<uint32_t>(w) : 0u,
			bitmap && h > 0 ? static_cast<uint32_t>(h) : 0u,
			bitmap,
			glm::vec2(static_cast<float>(xoff), static_cast<float>(yoff)),
			advance * scale);

		stbtt_FreeBitmap(bitmap, nullptr);
		return glyph;
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

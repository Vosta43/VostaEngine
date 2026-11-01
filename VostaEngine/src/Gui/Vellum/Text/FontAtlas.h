#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"   // TextureId
#include "Gui/Vellum/Text/GlyphAtlas.h"

#include <glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ve::vellum {

	class Font;
	class VellumRenderer;

	// One font at one pixel size: the metrics a text widget needs, plus the lookup
	// path into the renderer's shared GlyphAtlas. It owns no texture and no font
	// bytes — the chain and the atlas are shared, which is what keeps eight sizes
	// from meaning eight copies of a 10 MB face.
	//
	// A cache miss rasterises the glyph through the first face in the chain with
	// an outline for it and packs it into the shared atlas; both need a live GL
	// context, so instances are created through Font/FontManager, never directly.
	class VE_API FontAtlas {
	public:
		using Glyph = GlyphAtlas::Glyph;

		// The shared atlas' texture id, identical for every size and font.
		TextureId textureId() const;

		// Looks `codepoint` up in the shared atlas, rasterising and inserting it on
		// a miss. Returns never null. Codepoints no face has an outline for come
		// back with a zero-size quad and a real advance, so layout still flows.
		const Glyph* get(uint32_t codepoint);

		// One-line width and line box of `text`, decoded as UTF-8.
		glm::vec2 measure(const std::string& text);
		float ascent() const { return m_ascent; }
		float lineHeight() const { return m_lineHeight; }

	private:
		friend class Font;

		FontAtlas(Font& font, float pixelSize);

		Font* m_font = nullptr;
		float m_pixelSize = 0.0f;
		VellumRenderer* m_renderer = nullptr;

		// stbtt_ScaleForPixelHeight per face, in chain order.
		std::vector<float> m_scales;

		float m_ascent = 0.0f;
		float m_lineHeight = 0.0f;
	};

}

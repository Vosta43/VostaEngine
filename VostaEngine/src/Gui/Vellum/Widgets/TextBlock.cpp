#include "vepch.h"
#include "TextBlock.h"
#include "Gui/Vellum/Core/PaintContext.h"
#include "Gui/Vellum/Text/FontAtlas.h"
#include "Gui/Vellum/Text/Utf8.h"

namespace ve::vellum {

	TextBlock::TextBlock(FontAtlas* font, const std::string& text, const glm::vec4& color)
		: m_font(font)
		, m_text(text)
		, m_color(color) {}

	void TextBlock::setText(const std::string& text) {
		m_text = text;
		invalidateDesiredSize();
	}

	glm::vec2 TextBlock::computeDesiredSize() const {
		// measure() may rasterise unseen codepoints; that is fine, the atlas is
		// mutable through the borrowed pointer.
		return m_font ? m_font->measure(m_text) : glm::vec2(0.0f);
	}

	void TextBlock::arrange(const Rect& slot) {
		m_rect = slot;
	}

	void TextBlock::paint(PaintContext& ctx) {
		if (!m_font)
			return;

		// The pen rides the baseline, which sits `ascent` below the rect's top.
		float x = m_rect.pos.x;
		const float baseline = m_rect.pos.y + m_font->ascent();
		const TextureId texture = m_font->textureId();

		std::size_t i = 0;
		while (i < m_text.size()) {
			const uint32_t cp = utf8Next(m_text, i);
			if (cp == 0)
				continue;

			const FontAtlas::Glyph* g = m_font->get(cp);
			if (!g)
				continue;
			if (g->size.x > 0.0f && g->size.y > 0.0f) {
				const glm::vec2 topLeft = { x + g->bearing.x, baseline + g->bearing.y };
				ctx.drawQuad({ topLeft, g->size }, g->uv0, g->uv1, texture, m_color);
			}
			x += g->advance;
		}
	}

}

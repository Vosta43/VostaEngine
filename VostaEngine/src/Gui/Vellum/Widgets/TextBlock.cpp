#include "vepch.h"
#include "TextBlock.h"
#include "Gui/Vellum/Core/PaintContext.h"
#include "Gui/Vellum/Text/Font.h"
#include "Gui/Vellum/Text/FontAtlas.h"
#include "Gui/Vellum/Text/Utf8.h"

#include <algorithm>
#include <cmath>

namespace ve::vellum {

	TextBlock::TextBlock(FontAtlas* font, const std::string& text, const glm::vec4& color)
		: m_font(font)
		, m_text(text)
		, m_color(color) {}

	TextBlock::TextBlock(Font* font, float logicalPixelSize,
	                     const std::string& text, const glm::vec4& color)
		: m_fontChain(font)
		, m_logicalSize(logicalPixelSize)
		, m_text(text)
		, m_color(color) {}

	void TextBlock::setFont(FontAtlas* font) {
		m_font = font;
		m_fontChain = nullptr;
		m_logicalSize = 0.0f;
		invalidateDesiredSize();
	}

	void TextBlock::setText(const std::string& text) {
		m_text = text;
		invalidateDesiredSize();
	}

	FontAtlas* TextBlock::activeAtlas() const {
		if (!m_fontChain || m_logicalSize <= 0.0f)
			return m_font;
		// Whole physical pixels: the glyph cache lives for the layer's lifetime, so
		// a resize drag must not mint an atlas per fractional size.
		const float physical = std::max(1.0f, std::round(m_logicalSize * layoutScale()));
		return m_fontChain->at(physical);
	}

	float TextBlock::inverseScale() const {
		const float scale = layoutScale();
		return (m_fontChain && m_logicalSize > 0.0f && scale > 0.0f) ? 1.0f / scale : 1.0f;
	}

	glm::vec2 TextBlock::computeDesiredSize() const {
		// measure() may rasterise unseen codepoints; that is fine, the atlas is
		// mutable through the borrowed pointer.
		FontAtlas* atlas = activeAtlas();
		return atlas ? atlas->measure(m_text) * inverseScale() : glm::vec2(0.0f);
	}

	void TextBlock::arrange(const Rect& slot) {
		m_rect = slot;
	}

	void TextBlock::paint(PaintContext& ctx) {
		FontAtlas* atlas = activeAtlas();
		if (!atlas)
			return;

		// The pen rides the baseline, which sits `ascent` below the rect's top. Atlas
		// metrics are physical pixels; the block's rect is logical, so they come back
		// through inverseScale() and the renderer scales the quads up again.
		const float inv = inverseScale();
		float x = m_rect.pos.x;
		const float baseline = m_rect.pos.y + atlas->ascent() * inv;
		const TextureId texture = atlas->textureId();

		std::size_t i = 0;
		while (i < m_text.size()) {
			const uint32_t cp = utf8Next(m_text, i);
			if (cp == 0)
				continue;

			const FontAtlas::Glyph* g = atlas->get(cp);
			if (!g)
				continue;
			if (g->size.x > 0.0f && g->size.y > 0.0f) {
				const glm::vec2 topLeft = { x + g->bearing.x * inv, baseline + g->bearing.y * inv };
				ctx.drawQuad({ topLeft, g->size * inv }, g->uv0, g->uv1, texture, m_color);
			}
			x += g->advance * inv;
		}
	}

}

#include "vepch.h"
#include "TextBlock.h"
#include "Gui/Vellum/Core/PaintContext.h"
#include "Gui/Vellum/Text/Font.h"
#include "Gui/Vellum/Text/FontAtlas.h"
#include "Gui/Vellum/Text/Utf8.h"

#include <algorithm>
#include <cmath>
#include <utility>

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

	void TextBlock::setGradient(std::vector<GradientStop> stops, GradientAxis axis,
	                            float cyclesPerSecond) {
		m_gradientStops = std::move(stops);
		m_gradientAxis = axis;
		m_gradientSpeed = cyclesPerSecond;
	}

	void TextBlock::tick(float dt) {
		if (!m_gradientStops.empty() && m_gradientSpeed != 0.0f && dt > 0.0f) {
			m_gradientPhase += dt * m_gradientSpeed;
			m_gradientPhase -= std::floor(m_gradientPhase);
		}
		Widget::tick(dt);
	}

	glm::vec4 TextBlock::sampleGradient(float t) const {
		t += m_gradientPhase;
		t -= std::floor(t);

		const GradientStop& first = m_gradientStops.front();
		if (t <= first.position)
			return first.color;

		for (std::size_t i = 1; i < m_gradientStops.size(); ++i) {
			const GradientStop& stop = m_gradientStops[i];
			if (t > stop.position)
				continue;
			const GradientStop& prev = m_gradientStops[i - 1];
			const float span = stop.position - prev.position;
			const float k = span > 0.0f ? (t - prev.position) / span : 0.0f;
			return glm::mix(prev.color, stop.color, k);
		}
		return m_gradientStops.back().color;
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

		// Sample by each corner's position in the block, not in the glyph, so a tall
		// and a short glyph meet on the same colour at the same offset instead of
		// each running its own full ramp.
		const bool gradient = m_gradientStops.size() >= 2;
		const bool horizontal = m_gradientAxis == GradientAxis::Horizontal;

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
				const Rect quad{ topLeft, g->size * inv };
				if (horizontal && gradient && m_rect.size.x > 0.0f) {
					const float u0 = (quad.pos.x - m_rect.pos.x) / m_rect.size.x;
					const float u1 = (quad.pos.x + quad.size.x - m_rect.pos.x) / m_rect.size.x;
					const glm::vec4 left = sampleGradient(u0);
					const glm::vec4 right = sampleGradient(u1);
					ctx.drawQuad(quad, g->uv0, g->uv1, texture, left, right, right, left);
				} else if (gradient && m_rect.size.y > 0.0f) {
					const float v0 = (quad.pos.y - m_rect.pos.y) / m_rect.size.y;
					const float v1 = (quad.pos.y + quad.size.y - m_rect.pos.y) / m_rect.size.y;
					const glm::vec4 top = sampleGradient(v0);
					const glm::vec4 bottom = sampleGradient(v1);
					ctx.drawQuad(quad, g->uv0, g->uv1, texture, top, top, bottom, bottom);
				} else {
					ctx.drawQuad(quad, g->uv0, g->uv1, texture, m_color);
				}
			}
			x += g->advance * inv;
		}
	}

}

#include "vepch.h"
#include "InputBox.h"

#include "Gui/Vellum/Core/PaintContext.h"
#include "Gui/Vellum/Text/FontAtlas.h"
#include "Gui/Vellum/Text/Utf8.h"
#include "Keycodes.h"

#include <algorithm>

namespace ve::vellum {

	namespace {
		constexpr float kBorderWidth = 1.0f;
		constexpr float kPaddingX = 8.0f;
		constexpr float kCaretWidth = 2.0f;
	}

	InputBox::InputBox(FontAtlas* font, const glm::vec2& size)
		: m_font(font)
		, m_size(size) {}

	void InputBox::setFont(FontAtlas* font) {
		m_font = font;
		updateCaretMetrics();
	}

	void InputBox::setSize(const glm::vec2& size) {
		m_size = size;
		invalidateDesiredSize();
	}

	void InputBox::setText(std::string text) {
		m_text = std::move(text);
		// Snap to a codepoint boundary when the new string is shorter.
		m_caret = std::min(m_caret, m_text.size());
		while (m_caret < m_text.size() &&
		       (static_cast<unsigned char>(m_text[m_caret]) & 0xC0) == 0x80)
			++m_caret;
		updateCaretMetrics();
	}

	glm::vec2 InputBox::computeDesiredSize() const {
		return m_size;
	}

	Rect InputBox::innerRect() const {
		return {
			{ m_rect.pos.x + kBorderWidth, m_rect.pos.y + kBorderWidth },
			{ std::max(0.0f, m_rect.size.x - 2.0f * kBorderWidth),
			  std::max(0.0f, m_rect.size.y - 2.0f * kBorderWidth) }
		};
	}

	Rect InputBox::contentRect() const {
		const Rect inner = innerRect();
		return {
			{ inner.pos.x + kPaddingX, inner.pos.y },
			{ std::max(0.0f, inner.size.x - 2.0f * kPaddingX), inner.size.y }
		};
	}

	void InputBox::arrange(const Rect& slot) {
		m_rect = slot;
		updateCaretMetrics();
	}

	float InputBox::textOffsetX(const std::string& text) const {
		if (m_textAlign != TextAlign::Center || !m_font || text.empty())
			return 0.0f;
		const float width = m_font->measure(text).x;
		const float visible = contentRect().size.x;
		// A value that does not fit is left-anchored so the caret can scroll to it.
		return width < visible ? (visible - width) * 0.5f : 0.0f;
	}

	void InputBox::updateCaretMetrics() {
		m_caretContentX = m_font ? m_font->measure(m_text.substr(0, m_caret)).x : 0.0f;

		const float visible = contentRect().size.x;
		if (!m_font || visible <= 0.0f)
			return;

		// Centred text that fits has nothing to scroll for, and scrolling it would
		// defeat the centring.
		if (m_textAlign == TextAlign::Center && m_font->measure(m_text).x < visible) {
			m_scrollX = 0.0f;
			return;
		}

		// Keep the caret inside [0, visible]; scroll only ever moves right.
		if (m_caretContentX - m_scrollX + kCaretWidth > visible)
			m_scrollX = m_caretContentX + kCaretWidth - visible;
		if (m_caretContentX - m_scrollX < 0.0f)
			m_scrollX = m_caretContentX;
		m_scrollX = std::max(0.0f, m_scrollX);
	}

	void InputBox::placeCaretAt(float contentX) {
		if (!m_font) {
			m_caret = m_text.size();
			updateCaretMetrics();
			return;
		}

		// Pick the codepoint boundary whose pen position is nearest the click.
		float x = 0.0f;
		std::size_t i = 0;
		std::size_t best = 0;
		float bestDist = contentX < 0.0f ? -contentX : contentX;
		while (i < m_text.size()) {
			const uint32_t cp = utf8Next(m_text, i);
			if (cp) {
				if (const FontAtlas::Glyph* g = m_font->get(cp))
					x += g->advance;
			}
			const float dist = contentX > x ? contentX - x : x - contentX;
			if (dist < bestDist) {
				bestDist = dist;
				best = i;
			}
		}
		m_caret = best;
		updateCaretMetrics();
	}

	void InputBox::notifyTextChanged() {
		if (m_onTextChanged)
			m_onTextChanged(m_text);
	}

	Reply InputBox::onMouseDown(const glm::vec2& pos) {
		const Rect content = contentRect();
		// The same inset the frame painted with, so a click lands under the glyph
		// it is visually over.
		const float offset = textOffsetX(m_text.empty() ? m_placeholder : m_text);
		placeCaretAt(pos.x - content.pos.x - offset + m_scrollX);
		return Reply::handleFocus();
	}

	bool InputBox::onKeyPressed(int key, int) {
		switch (key) {
		case VE_KEY_BACKSPACE:
			if (m_caret > 0) {
				const std::size_t start = utf8Prev(m_text, m_caret);
				m_text.erase(start, m_caret - start);
				m_caret = start;
				notifyTextChanged();
				updateCaretMetrics();
			}
			return true;

		case VE_KEY_DELETE:
			if (m_caret < m_text.size()) {
				std::size_t next = m_caret;
				utf8Next(m_text, next);
				m_text.erase(m_caret, next - m_caret);
				notifyTextChanged();
				updateCaretMetrics();
			}
			return true;

		case VE_KEY_LEFT:
			m_caret = utf8Prev(m_text, m_caret);
			updateCaretMetrics();
			return true;

		case VE_KEY_RIGHT: {
			std::size_t next = m_caret;
			utf8Next(m_text, next);
			m_caret = next;
			updateCaretMetrics();
			return true;
		}

		case VE_KEY_HOME:
			m_caret = 0;
			updateCaretMetrics();
			return true;

		case VE_KEY_END:
			m_caret = m_text.size();
			updateCaretMetrics();
			return true;

		case VE_KEY_ENTER:
		case VE_KEY_KP_ENTER:
			if (m_onSubmit)
				m_onSubmit(m_text);
			return true;

		default:
			return false;
		}
	}

	bool InputBox::onTextInput(uint32_t codepoint) {
		if (codepoint == 0)
			return true;

		std::string encoded;
		utf8Append(encoded, codepoint);
		m_text.insert(m_caret, encoded);
		m_caret += encoded.size();
		notifyTextChanged();
		updateCaretMetrics();
		return true;
	}

	void InputBox::onFocus() {
		updateCaretMetrics();
	}

	void InputBox::paint(PaintContext& ctx) {
		const Rect inner = innerRect();
		if (m_bgTexture) {
			// Pixel-art background: the focus accent is a tint, never a new border.
			glm::vec4 tint = m_bgTint;
			if (isFocused()) {
				tint.r *= m_focusTintScale;
				tint.g *= m_focusTintScale;
				tint.b *= m_focusTintScale;
			}
			ctx.drawQuad(m_rect, glm::vec2(0.0f), glm::vec2(1.0f), m_bgTexture, tint);
		}
		else {
			const glm::vec4& border = isFocused() ? m_focusBorderColor
			                        : isHovered()  ? m_hoverBorderColor
			                        :                m_borderColor;
			ctx.fillRect(m_rect, border);
			ctx.fillRect(inner, m_bgColor);
		}
		if (!m_font)
			return;

		// An empty field reads as its placeholder; typing replaces it in place.
		const bool empty = m_text.empty();
		const std::string& display = empty ? m_placeholder : m_text;
		if (display.empty() && !isFocused())
			return;

		const Rect content = contentRect();
		ctx.pushClip(content);

		const float lineTop = inner.pos.y +
			std::max(0.0f, (inner.size.y - m_font->lineHeight()) * 0.5f);
		const float baseline = lineTop + m_font->ascent();
		const TextureId texture = m_font->textureId();
		const float offset = textOffsetX(display);

		float x = content.pos.x + offset - m_scrollX;
		std::size_t i = 0;
		while (i < display.size()) {
			const uint32_t cp = utf8Next(display, i);
			if (cp == 0)
				continue;

			const FontAtlas::Glyph* g = m_font->get(cp);
			if (!g)
				continue;
			if (g->size.x > 0.0f && g->size.y > 0.0f) {
				const glm::vec2 topLeft = { x + g->bearing.x, baseline + g->bearing.y };
				ctx.drawQuad({ topLeft, g->size }, g->uv0, g->uv1, texture,
				             empty ? m_placeholderColor : m_textColor);
			}
			x += g->advance;
		}

		if (isFocused()) {
			const float caretX = content.pos.x + offset + m_caretContentX - m_scrollX;
			ctx.fillRect({ { caretX, lineTop }, { kCaretWidth, m_font->lineHeight() } },
			             m_caretColor);
		}

		ctx.popClip();
	}

}

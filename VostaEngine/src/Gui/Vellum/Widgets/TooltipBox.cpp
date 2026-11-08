#include "vepch.h"
#include "TooltipBox.h"
#include "Gui/Vellum/Core/PaintContext.h"

namespace ve::vellum {

	namespace {
		constexpr float kBorder = 1.0f;
	}

	TooltipBox::TooltipBox(FontAtlas* font) {
		m_text = CreateRef<TextBlock>(font, std::string(), m_textColor);
		addChild(m_text);
	}

	void TooltipBox::setFont(FontAtlas* font) {
		m_text->setFont(font);
	}

	void TooltipBox::setText(const std::string& text) {
		if (text == m_text->text())
			return;
		m_text->setText(text);
		invalidateDesiredSize();
	}

	void TooltipBox::setPadding(const glm::vec2& padding) {
		if (padding == m_padding)
			return;
		m_padding = padding;
		invalidateDesiredSize();
	}

	void TooltipBox::setTextColor(const glm::vec4& color) {
		m_textColor = color;
		m_text->setColor(color);
	}

	glm::vec2 TooltipBox::computeDesiredSize() const {
		if (m_text->text().empty())
			return glm::vec2(0.0f);
		return m_text->desiredSize() + m_padding * 2.0f + glm::vec2(kBorder * 2.0f);
	}

	void TooltipBox::arrange(const Rect& slot) {
		m_rect = slot;

		const glm::vec2 size = desiredSize();
		if (size.x <= 0.0f || size.y <= 0.0f)
			return;

		// Hang off the anchor, then fold back inside the surface so a bubble near an
		// edge stays readable instead of running off-screen.
		glm::vec2 pos = m_anchor + m_offset;
		const glm::vec2 limit = slot.pos + slot.size - size;
		pos.x = glm::clamp(pos.x, slot.pos.x, glm::max(slot.pos.x, limit.x));
		pos.y = glm::clamp(pos.y, slot.pos.y, glm::max(slot.pos.y, limit.y));
		m_rect = { pos, size };

		m_text->arrange({ pos + glm::vec2(kBorder) + m_padding, m_text->desiredSize() });
	}

	void TooltipBox::onPaint(PaintContext& ctx) {
		if (m_rect.size.x <= 0.0f || m_rect.size.y <= 0.0f)
			return;

		ctx.fillRect(m_rect, m_background);
		const glm::vec2& p = m_rect.pos;
		const glm::vec2& s = m_rect.size;
		ctx.fillRect({ { p.x, p.y }, { s.x, kBorder } }, m_border);
		ctx.fillRect({ { p.x, p.y + s.y - kBorder }, { s.x, kBorder } }, m_border);
		ctx.fillRect({ { p.x, p.y }, { kBorder, s.y } }, m_border);
		ctx.fillRect({ { p.x + s.x - kBorder, p.y }, { kBorder, s.y } }, m_border);

		Widget::onPaint(ctx);
	}

}

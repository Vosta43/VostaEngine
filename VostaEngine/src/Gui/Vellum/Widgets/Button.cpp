#include "vepch.h"
#include "Button.h"
#include "Gui/Vellum/Core/PaintContext.h"

namespace ve::vellum {

Button::Button(const glm::vec2& size, const glm::vec4& color)
	: m_color(color)
	, m_size(size) {
	// Derive the interaction states from the base colour so a caller only picks one.
	m_hoverColor = glm::min(color * 1.35f, glm::vec4(1.0f, 1.0f, 1.0f, color.a));
	m_hoverColor.a = color.a;
	m_pressedColor = glm::vec4(color.r * 0.7f, color.g * 0.7f, color.b * 0.7f, color.a);
}

glm::vec2 Button::computeDesiredSize() const {
	return m_size;
}

void Button::arrange(const Rect& slot) {
	m_rect = slot;
	// The label (any child) keeps its measured size and centres in the button.
	for (const auto& child : m_children) {
		const glm::vec2 size = child->desiredSize();
		child->arrange({ slot.pos + (slot.size - size) * 0.5f, size });
	}
}

void Button::paint(PaintContext& ctx) {
	const glm::vec4& color = isPressed() ? m_pressedColor
	                      : isHovered()  ? m_hoverColor
	                      :                m_color;
	ctx.fillRect(m_rect, color);
	Widget::paint(ctx);
}

Reply Button::onMouseDown(const glm::vec2&) {
	return Reply::handleAndCapture();
}

Reply Button::onMouseUp(const glm::vec2& pos) {
	// Releasing outside the rect cancels the click; the capture delivered the up
	// here regardless of where the cursor ended up.
	if (containsPoint(pos) && m_onClick)
		m_onClick();
	return Reply::handle();
}

}

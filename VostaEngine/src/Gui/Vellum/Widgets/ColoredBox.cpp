#include "vepch.h"
#include "ColoredBox.h"
#include "Gui/Vellum/Core/PaintContext.h"

namespace ve::vellum {

ColoredBox::ColoredBox(const glm::vec4& color, const glm::vec2& size)
	: m_color(color)
	, m_size(size) {}

void ColoredBox::setSize(const glm::vec2& size) {
	m_size = size;
	invalidateDesiredSize();
}

glm::vec2 ColoredBox::computeDesiredSize() const {
	glm::vec2 content{ 0.0f };
	for (const auto& child : m_children)
		content = glm::max(content, child->desiredSize());
	return glm::max(m_size, content);
}

void ColoredBox::arrange(const Rect& slot) {
	m_rect = slot;
}

void ColoredBox::paint(PaintContext& ctx) {
	ctx.fillRect(m_rect, m_color);
	Widget::paint(ctx);  // children (if any) draw on top
}

}

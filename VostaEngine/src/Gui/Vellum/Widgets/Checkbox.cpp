#include "vepch.h"
#include "Checkbox.h"
#include "Gui/Vellum/Core/PaintContext.h"

namespace ve::vellum {

	Checkbox::Checkbox(bool value, const glm::vec2& size)
		: m_size(size)
		, m_value(value) {}

	glm::vec2 Checkbox::computeDesiredSize() const {
		return m_size;
	}

	void Checkbox::setSize(const glm::vec2& size) {
		m_size = size;
		invalidateDesiredSize();
	}

	void Checkbox::arrange(const Rect& slot) {
		m_rect = slot;
		for (const auto& child : m_children) {
			const glm::vec2 size = child->desiredSize();
			child->arrange({ slot.pos + (slot.size - size) * 0.5f, size });
		}
	}

	void Checkbox::tick(float dt) {
		m_hoverAnim.to(isHovered() ? 1.0f : 0.0f);
		m_pressAnim.to(isPressed() ? 1.0f : 0.0f);
		m_hoverAnim.tick(dt);
		m_pressAnim.tick(dt);
		Widget::tick(dt);
	}

	void Checkbox::paint(PaintContext& ctx) {
		TextureId texture = m_value ? m_style.onTexture : m_style.offTexture;
		if (texture == 0)
			texture = m_value ? m_style.offTexture : m_style.onTexture;

		glm::vec4 tint = glm::mix(m_style.normalTint, m_style.hoveredTint, m_hoverAnim.value);
		tint = glm::mix(tint, m_style.pressedTint, m_pressAnim.value);
		ctx.drawQuad(m_rect, glm::vec2(0.0f), glm::vec2(1.0f), texture, tint);

		Widget::paint(ctx);
	}

	Reply Checkbox::onMouseDown(const glm::vec2&) {
		m_value = !m_value;
		if (m_onValueChanged)
			m_onValueChanged(m_value);
		return Reply::handleAndCapture();
	}

	Reply Checkbox::onMouseUp(const glm::vec2&) {
		return Reply::handle();
	}

}

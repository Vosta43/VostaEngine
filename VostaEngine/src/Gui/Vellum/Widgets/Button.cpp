#include "vepch.h"
#include "Button.h"
#include "Gui/Vellum/Core/PaintContext.h"

namespace ve::vellum {

	Button::Button(const glm::vec2& size, const glm::vec4& color)
		: m_style(ButtonStyle::fromColor(color))
		, m_size(size) {}

	glm::vec2 Button::computeDesiredSize() const {
		return m_size;
	}

	void Button::setSize(const glm::vec2& size) {
		m_size = size;
		invalidateDesiredSize();
	}

	void Button::setTexture(State state, TextureId texture) {
		switch (state) {
		case State::Normal:  m_style.normalTexture  = texture; break;
		case State::Hovered: m_style.hoveredTexture = texture; break;
		case State::Pressed: m_style.pressedTexture = texture; break;
		}
	}

	void Button::arrange(const Rect& slot) {
		m_rect = slot;
		// The label (any child) keeps its measured size and centres in the button.
		for (const auto& child : m_children) {
			const glm::vec2 size = child->desiredSize();
			child->arrange({ slot.pos + (slot.size - size) * 0.5f, size });
		}
	}

	void Button::tick(float dt) {
		m_hoverAnim.to(isHovered() ? 1.0f : 0.0f);
		m_pressAnim.to(isPressed() ? 1.0f : 0.0f);
		m_hoverAnim.tick(dt);
		m_pressAnim.tick(dt);
		Widget::tick(dt);
	}

	void Button::paint(PaintContext& ctx) {
		TextureId texture = isPressed() ? m_style.pressedTexture
		                  : isHovered() ? m_style.hoveredTexture
		                  : m_style.normalTexture;
		if (texture == 0)
			texture = m_style.normalTexture;

		glm::vec4 tint = glm::mix(m_style.normalTint, m_style.hoveredTint, m_hoverAnim.value);
		tint = glm::mix(tint, m_style.pressedTint, m_pressAnim.value);
		ctx.drawQuad(m_rect, glm::vec2(0.0f), glm::vec2(1.0f), texture, tint);

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

#include "vepch.h"
#include "Slider.h"
#include "Gui/Vellum/Core/PaintContext.h"

#include <algorithm>

namespace ve::vellum {

	namespace {
		float clampValue(float v) {
			return std::clamp(v, 0.0f, 1.0f);
		}
	}

	Slider::Slider(float value, const glm::vec2& size)
		: m_size(size)
		, m_value(clampValue(value)) {}

	glm::vec2 Slider::computeDesiredSize() const {
		return m_size;
	}

	void Slider::setSize(const glm::vec2& size) {
		m_size = size;
		invalidateDesiredSize();
	}

	void Slider::setValue(float value) {
		m_value = clampValue(value);
	}

	void Slider::arrange(const Rect& slot) {
		m_rect = slot;
		for (const auto& child : m_children) {
			const glm::vec2 size = child->desiredSize();
			child->arrange({ slot.pos + (slot.size - size) * 0.5f, size });
		}
	}

	void Slider::tick(float dt) {
		m_hoverAnim.to(isHovered() ? 1.0f : 0.0f);
		m_pressAnim.to(isPressed() ? 1.0f : 0.0f);
		m_hoverAnim.tick(dt);
		m_pressAnim.tick(dt);
		Widget::tick(dt);
	}

	void Slider::onPaint(PaintContext& ctx) {
		glm::vec4 tint = glm::mix(m_style.normalTint, m_style.hoveredTint, m_hoverAnim.value);
		tint = glm::mix(tint, m_style.pressedTint, m_pressAnim.value);

		ctx.drawQuad(m_rect, glm::vec2(0.0f), glm::vec2(1.0f), m_style.trackTexture, tint);

		const float knobSize = m_rect.size.y * m_style.knobRatio;
		const float knobX = m_rect.pos.x + m_value * m_rect.size.x - knobSize * 0.5f;
		const float knobY = m_rect.pos.y + (m_rect.size.y - knobSize) * 0.5f;
		ctx.drawQuad({ { knobX, knobY }, { knobSize, knobSize } },
		             glm::vec2(0.0f), glm::vec2(1.0f), m_style.knobTexture, tint);

		Widget::onPaint(ctx);
	}

	void Slider::applyDrag(float x) {
		if (m_rect.size.x <= 0.0f)
			return;

		const float next = clampValue((x - m_rect.pos.x) / m_rect.size.x);
		if (next == m_value)
			return;

		m_value = next;
		if (m_onValueChanged)
			m_onValueChanged(m_value);
	}

	Reply Slider::onMouseDown(const glm::vec2& pos) {
		applyDrag(pos.x);
		return Reply::handleAndCapture();
	}

	Reply Slider::onMouseMove(const glm::vec2& pos) {
		applyDrag(pos.x);
		return Reply::handle();
	}

	Reply Slider::onMouseUp(const glm::vec2&) {
		return Reply::handle();
	}

}

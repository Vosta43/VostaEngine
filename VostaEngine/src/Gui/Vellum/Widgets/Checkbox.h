#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Animated.h"
#include "Gui/Vellum/Core/DrawList.h"
#include "Gui/Vellum/Core/Reply.h"
#include "Gui/Vellum/Core/Widget.h"

#include <functional>

namespace ve::vellum {

	// Appearance of one checkbox. The value picks which texture is drawn; hover
	// and press only tint it, so a two-state switch needs no extra art.
	struct CheckboxStyle {
		TextureId onTexture  = 0;
		TextureId offTexture = 0;

		glm::vec4 normalTint { 1.0f };
		glm::vec4 hoveredTint{ 1.2f, 1.2f, 1.2f, 1.0f };
		glm::vec4 pressedTint{ 0.8f, 0.8f, 0.8f, 1.0f };

		static CheckboxStyle fromTextures(TextureId on, TextureId off,
		                                  const glm::vec4& tint = glm::vec4(1.0f)) {
			CheckboxStyle style;
			style.onTexture  = on;
			style.offTexture = off;
			style.normalTint = tint;
			return style;
		}
	};

	// Two-texture boolean switch. A press flips the value and reports it, matching
	// the legacy UICheckbox (which toggled on press, not release); the release only
	// ends the capture. setValue() is the host-side write and fires no callback.
	class VE_API Checkbox : public Widget {
	public:
		Checkbox(bool value, const glm::vec2& size);

		void setSize(const glm::vec2& size);

		void setStyle(const CheckboxStyle& style) { m_style = style; }
		const CheckboxStyle& style() const { return m_style; }

		bool value() const { return m_value; }
		void setValue(bool value) { m_value = value; }

		void setOnValueChanged(std::function<void(bool)> fn) { m_onValueChanged = std::move(fn); }

		// Fade speed of the hover/press tint, in e-folds per second.
		void setTransitionRate(float rate) {
			m_hoverAnim.rate = rate;
			m_pressAnim.rate = rate;
		}

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;
		void tick(float dt) override;

		Reply onMouseDown(const glm::vec2& pos) override;
		Reply onMouseUp(const glm::vec2& pos) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		CheckboxStyle m_style;
		glm::vec2     m_size{ 0.0f };
		bool          m_value = false;
		std::function<void(bool)> m_onValueChanged;
		AnimFloat     m_hoverAnim;
		AnimFloat     m_pressAnim;
	};

}

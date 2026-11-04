#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Animated.h"
#include "Gui/Vellum/Core/DrawList.h"
#include "Gui/Vellum/Core/Reply.h"
#include "Gui/Vellum/Core/Widget.h"

#include <functional>

namespace ve::vellum {

	// Appearance of one horizontal slider: a track stretched over the whole rect
	// and a square knob riding on it. The knob is `rect.height * knobRatio` wide,
	// which is the legacy UISlider's geometry.
	struct SliderStyle {
		TextureId trackTexture = 0;
		TextureId knobTexture  = 0;

		glm::vec4 normalTint { 1.0f };
		glm::vec4 hoveredTint{ 1.2f, 1.2f, 1.2f, 1.0f };
		glm::vec4 pressedTint{ 0.8f, 0.8f, 0.8f, 1.0f };

		float knobRatio = 0.5f;

		static SliderStyle fromTextures(TextureId track, TextureId knob,
		                                const glm::vec4& tint = glm::vec4(1.0f)) {
			SliderStyle style;
			style.trackTexture = track;
			style.knobTexture  = knob;
			style.normalTint   = tint;
			return style;
		}
	};

	// Draggable 0..1 value. Pressing anywhere on the track jumps the value there
	// and captures the pointer; moves then follow the cursor until release.
	class VE_API Slider : public Widget {
	public:
		Slider(float value, const glm::vec2& size);

		void setSize(const glm::vec2& size);

		void setStyle(const SliderStyle& style) { m_style = style; }
		const SliderStyle& style() const { return m_style; }

		float value() const { return m_value; }
		// Host-side write; clamped and does not fire the callback.
		void setValue(float value);

		void setOnValueChanged(std::function<void(float)> fn) { m_onValueChanged = std::move(fn); }

		void setTransitionRate(float rate) {
			m_hoverAnim.rate = rate;
			m_pressAnim.rate = rate;
		}

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;
		void tick(float dt) override;

		Reply onMouseDown(const glm::vec2& pos) override;
		Reply onMouseMove(const glm::vec2& pos) override;
		Reply onMouseUp(const glm::vec2& pos) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		void applyDrag(float x);

		SliderStyle m_style;
		glm::vec2   m_size{ 0.0f };
		float       m_value = 0.0f;
		std::function<void(float)> m_onValueChanged;
		AnimFloat   m_hoverAnim;
		AnimFloat   m_pressAnim;
	};

}

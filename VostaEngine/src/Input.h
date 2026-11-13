#pragma once

#include "Core/Core.h"
#include "Core/InputGate.h"

namespace ve {
	class VE_API Input {
	public:
		inline static bool isKeyPressed(int keycode) { return inputBlocked() ? false : s_instance->isKeyPressedImpl(keycode); }
		inline static bool isMousePressed(int button) { return inputBlocked() ? false : s_instance->isMousePressedImpl(button); }
		inline static float getMouseX() { return inputBlocked() ? 0.0f : s_instance->getMouseXImpl(); }
		inline static float getMouseY() { return inputBlocked() ? 0.0f : s_instance->getMouseYImpl(); }
		inline static std::pair<float, float> getMousePosition() { return inputBlocked() ? std::pair<float, float>(0.0f, 0.0f) : s_instance->getMousePositionImpl(); }

	protected:
		virtual bool isKeyPressedImpl(int keycode) = 0;
		virtual bool isMousePressedImpl(int button) = 0;
		virtual float getMouseXImpl() = 0;
		virtual float getMouseYImpl() = 0;
		virtual std::pair<float,float> getMousePositionImpl() = 0;
	private:
		static Input* s_instance;

	};
}

#pragma once

#include "Core/Core.h"

namespace ve {
	class VE_API Input {
	public:
		inline static bool isKeyPressed(int keycode) {return s_instance->isKeyPressedImpl(keycode); }
		inline static bool isMousePressed(int button) { return s_instance->isMousePressedImpl(button); }
		inline static float getMouseX() { return s_instance->getMouseXImpl(); }
		inline static float getMouseY() { return s_instance->getMouseYImpl(); }
		inline static std::pair<float, float> getMousePosition() {return s_instance->getMousePositionImpl(); }

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
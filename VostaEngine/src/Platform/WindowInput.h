#pragma once

#include "../Input.h"

namespace ve {
	

	class WindowsInput : public Input{
	public:

	protected:
		bool isKeyPressedImpl(int keycode) override;
		bool isMousePressedImpl(int button) override;
		float getMouseXImpl() override;
		float getMouseYImpl() override;
		std::pair<float, float> getMousePositionImpl() override;
	};

}
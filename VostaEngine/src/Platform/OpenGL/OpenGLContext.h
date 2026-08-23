#pragma once

#include "Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace ve {
	class OpenGLContext : public GraphicsContext {
	public:
		OpenGLContext(GLFWwindow* windowHandle);
		void init() override;
		void swapBuffers() override;

	private:
		GLFWwindow* m_windowHandle;
	};


}
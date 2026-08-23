#include "vepch.h"
#include "OpenGLContext.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace ve {
	
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		:m_windowHandle(windowHandle){
			
	}

	void OpenGLContext::init() {
		glfwMakeContextCurrent(m_windowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		if (!status) {
			VE_CORE_ERROR_PRINT("Failed to initialize GLAD", "");
		}
	}

	void OpenGLContext::swapBuffers(){
		glfwSwapBuffers(m_windowHandle);
	}

}
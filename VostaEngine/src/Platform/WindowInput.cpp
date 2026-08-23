#include "vepch.h"
#include "WindowInput.h"
#include <GLFW/glfw3.h>

#include "../Core/Application.h"
#include "../Core/Window.h"

namespace ve {
    
    Input* Input::s_instance = new WindowsInput();

    bool WindowsInput::isKeyPressedImpl(int keycode){
	
	    auto window = static_cast<GLFWwindow*>(Application::get().getWindow()->getNativeWindow());
	    auto state = glfwGetKey(window,keycode);
	    return state == GLFW_PRESS || state == GLFW_REPEAT;
	    return false;
    }

    bool WindowsInput::isMousePressedImpl(int button) {
        auto window = Application::get().getWindow()->getNativeWindow();
        auto state = glfwGetMouseButton(window, button);
        return state == GLFW_PRESS;
    }

    float WindowsInput::getMouseXImpl() {
        auto window = Application::get().getWindow()->getNativeWindow();
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        return static_cast<float>(xpos);
    }

    float WindowsInput::getMouseYImpl() {
        auto window = Application::get().getWindow()->getNativeWindow();
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        return static_cast<float>(ypos);
    }

    std::pair<float, float> WindowsInput::getMousePositionImpl(){
        auto window = Application::get().getWindow()->getNativeWindow();
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        return std::pair<float, float>(xpos,ypos);
    }

}

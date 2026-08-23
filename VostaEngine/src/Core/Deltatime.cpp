#include "vepch.h"
#include "DeltaTime.h"
#include <GLFW/glfw3.h>

namespace ve {

    DeltaTime::DeltaTime()
        : m_lastFrameTime(0.0f)
        , m_deltaTime(0.0f) {
    }

    void DeltaTime::update() {
        float currentFrame = static_cast<float>(glfwGetTime());
        m_deltaTime = currentFrame - m_lastFrameTime;
        m_lastFrameTime = currentFrame;
    }

    float DeltaTime::getDeltaTime() const {
        return m_deltaTime;
    }

    float DeltaTime::getCurrentTime() const {
        return static_cast<float>(glfwGetTime());
    }

}
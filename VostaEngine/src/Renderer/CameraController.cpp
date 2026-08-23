#include "vepch.h"
#include "CameraController.h"
#include "Input.h"
#include "KeyCodes.h"
#include "Core/Events/MouseMovedEvent.h"
#include "Core/Events/MouseScrolledEvent.h"


namespace ve {

    CameraController::CameraController(float left, float right, float bottom, float top)
        : m_camera(left, right, bottom, top) {
    }

    void CameraController::setActive(bool active) {
        m_active = active;
        if (!active) {
            // Reset mouse state so the cursor position is re-initialized on the next activation (prevents a sudden jump).
            m_firstMouse = true; 
        }
    }

    void CameraController::onUpdate(float deltaTime) {
        
        if(!m_active)return;

        auto [mouseX, mouseY] = Input::getMousePosition();

        if (m_firstMouse) {
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
            m_firstMouse = false;
        }

        float deltaX = (mouseX - m_lastMouseX) * m_mouseSensitivity;
        float deltaY = (mouseY - m_lastMouseY) * m_mouseSensitivity;
        m_lastMouseX = mouseX;
        m_lastMouseY = mouseY;

        m_camera.rotate(deltaX, -deltaY);


        float moveSpeed = m_moveSpeed * deltaTime;
        glm::vec3 movement(0.0f);

        if (Input::isKeyPressed(VE_KEY_W)) movement += m_camera.getFront();
        if (Input::isKeyPressed(VE_KEY_S)) movement -= m_camera.getFront();
        if (Input::isKeyPressed(VE_KEY_A)) movement -= m_camera.getRight();
        if (Input::isKeyPressed(VE_KEY_D)) movement += m_camera.getRight();
        const float rollSpeed = 6.0f;
        if (Input::isKeyPressed(VE_KEY_J))
            m_camera.addRoll(-rollSpeed * deltaTime);  // J: Roll left
        if (Input::isKeyPressed(VE_KEY_L))
            m_camera.addRoll(rollSpeed * deltaTime);   // J: Roll right

        if (glm::length(movement) > 0.0f)
            movement = glm::normalize(movement) * moveSpeed;

        m_camera.setPosition(m_camera.getPosition() + movement);
    }

    void CameraController::onMouseScrolled(float xOffset, float yOffset) {
        float fov = m_camera.getPerspectiveFov();
        fov -= yOffset * 2.0f; 
        if (fov < 10.0f) fov = 10.0f;
        if (fov > 120.0f) fov = 120.0f;
        m_camera.setPerspectiveFov(fov);
    }

    void CameraController::onEvent(Event& event) {
        if (event.getType() == Event::Type::MouseScrolled) {
            auto& e = static_cast<MouseScrolledEvent&>(event);
            onMouseScrolled(e.getXOffset(), e.getYOffset());
        }
    }

}
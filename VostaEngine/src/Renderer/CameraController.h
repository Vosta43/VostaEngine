#pragma once

#include "Camera.h"
#include "Core/Events/Event.h"

namespace ve {

    class VE_API CameraController {
    public:
        CameraController(float left, float right, float bottom, float top);

        void setActive(bool active);

        void onUpdate(float deltaTime);
        void onEvent(Event& event);

        Camera& getCamera() { return m_camera; }
        const Camera& getCamera() const { return m_camera; }

        void onMouseScrolled(float xOffset, float yOffset);

        float getMoveSpeed() const { return m_moveSpeed; }
        void setMoveSpeed(float speed) { m_moveSpeed = speed; }

    private:
        Camera m_camera;

        float m_zoomLevel = 1.0f;
        float m_zoomSpeed = 0.1f;

        float m_moveSpeed = 12.0f;
        float m_mouseSensitivity = 0.1f;

        float m_lastMouseX = 0.0f;
        float m_lastMouseY = 0.0f;
        bool  m_firstMouse = true;

        bool m_active;
    };

}
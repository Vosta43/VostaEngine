#pragma once
#include "Core/Log.h"
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

namespace ve {

    class VE_API Camera {
    public:
        Camera(float left, float right, float bottom, float top)
            : m_projectionMatrix(glm::ortho(left, right, bottom, top, -1.0f, 100.0f))
            , m_viewMatrix(1.0f)
            , m_position(0.0f)
            , m_rotation(0.0f)
            , m_perspectiveFov(45.0f) {

            recalculateProjection();
        }
        void setAspectRatio(float aspectRatio) {
            m_aspectRatio = aspectRatio;
            recalculateProjection();
        }
        
        void setFarPlane(float fp) {
            m_farPlane = fp;
            recalculateProjection();
        }

        float getFarPlane() {
            return m_farPlane;
        }

        void recalculateProjection() {
            //VE_CORE_SUCCESS_PRINT("FOV=%.2f Aspect=%.2f", m_perspectiveFov, m_aspectRatio);
            if (m_isPerspective) {
                m_projectionMatrix = glm::perspective(glm::radians(m_perspectiveFov), m_aspectRatio, m_nearPlane, m_farPlane);
            }
            else {
                float halfHeight = (m_top - m_bottom) * 0.5f;
                float halfWidth = halfHeight * m_aspectRatio;
                m_projectionMatrix = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1.0f, 100.0f);
            }
        }

        void setPosition(const glm::vec3& position) {
            m_position = position;
            recalculateViewMatrix();
        }

        const glm::vec3& getPosition() const { return m_position; }

        float getYaw() const { return m_yaw; }
        float getPitch() const { return m_pitch; }

        void setRotation(float rotation) {
            m_rotation = rotation;
            recalculateViewMatrix();
        }

        void setProjectionType(bool perspective) {
            m_isPerspective = perspective;
            recalculateProjection();
        }

        // Is for jitter calculation.
        void setViewportSize(uint32_t w, uint32_t h) {
            m_viewportWidth = w;
            m_viewportHeight = h;
        }

        const glm::mat4& getPrevViewProjectionMatrix() const {
            return m_prevViewProjectionMatrix;
        }

        // The same counter that drives the halton projection jitter: frame N
        // here is the same temporal sample the cloud blue-noise phase is meant
        // to match, so both TAA stacks share one time base.
        uint32_t getFrameIndex() const { return m_frameIndex; }

        glm::mat4 getJitteredProjectionMatrix() const;
        // Store prevVP and frameIndex++
        void endFrame();

        void setJitterEnabled(bool b) { m_jitterEnabled = b; }

        const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }
        const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
        glm::mat4 getViewProjectionMatrix() const { return m_projectionMatrix * m_viewMatrix; }

        void setYawPitch(float yaw, float pitch) {
            m_yaw = yaw;
            m_pitch = pitch;
            recalculateViewMatrix();
        }

        void rotate(float deltaYaw, float deltaPitch) {
            m_yaw -= deltaYaw; 
            m_pitch += deltaPitch;

            if (m_pitch > 89.0f) m_pitch = 89.0f;
            if (m_pitch < -89.0f) m_pitch = -89.0f;

            recalculateViewMatrix();
        }

        bool isPerspective() const {return m_isPerspective;}

        float getPerspectiveFov() const {return m_perspectiveFov;}

        void setPerspectiveFov(float fov) {
            m_perspectiveFov = fov;
            recalculateProjection();
        }

        void setOrthoBounds(float left, float right, float bottom, float top) {
            m_left = left; m_right = right; m_bottom = bottom; m_top = top;
            recalculateProjection();
        }

        void addRoll(float deltaRoll) {
            m_roll += deltaRoll;
            recalculateViewMatrix();
        }

        glm::vec3 getFront() const {
            glm::quat pitchQuat = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat yawQuat = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat rollQuat = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::quat orientation = yawQuat * pitchQuat * rollQuat;
            return glm::normalize(orientation * glm::vec3(0.0f, 0.0f, -1.0f));
        }

        glm::vec3 getRight() const {
            return glm::normalize(glm::cross(getFront(), glm::vec3(0.0f, 1.0f, 0.0f)));
        }
        

    private:
        void recalculateViewMatrix() {
            glm::quat pitchQuat = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat yawQuat = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat rollQuat = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::quat orientation = yawQuat * pitchQuat * rollQuat;

            glm::vec3 front = orientation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up = orientation * glm::vec3(0.0f, 1.0f, 0.0f);

            m_viewMatrix = glm::lookAt(m_position, m_position + front, up);
        }
    private:

        glm::mat4 m_projectionMatrix;
        glm::mat4 m_viewMatrix;

        glm::vec3 m_position;
        float m_rotation;
        float m_yaw = -90.0f;
        float m_pitch = 0.0f;
        float m_roll = 0.0f;
        float m_aspectRatio = 1.0f;
        float m_perspectiveFov;

        float m_nearPlane = 0.1f;
        float m_farPlane = 1200.0f;

        float m_left, m_right, m_bottom, m_top;
        bool m_isPerspective = false;

        glm::mat4 m_prevViewProjectionMatrix = glm::mat4(1.0f);
        uint32_t m_frameIndex = 0;
        bool m_jitterEnabled = true;
        uint32_t m_viewportWidth = 1,m_viewportHeight = 1;
    };

}
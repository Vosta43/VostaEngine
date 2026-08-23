#include "vepch.h"
#include "Camera.h"

namespace ve {
    // TODO:Figure out
    static float halton(uint32_t index, uint32_t base) {
        float f = 1.0f;
        float r = 0.0f;
        while (index > 0) {
            f /= (float)base;
            r += f * (float)(index % base);
            index /= base;
        }
        return r;
    }

    glm::mat4 Camera::getJitteredProjectionMatrix() const {
        if (!m_jitterEnabled) return m_projectionMatrix;
        float jx = halton(m_frameIndex, 2) - 0.5f;
        float jy = halton(m_frameIndex, 3) - 0.5f;
        glm::mat4 proj = m_projectionMatrix;
        proj[2][0] += jx * 2.0f / (float)m_viewportWidth;
        proj[2][1] += jy * 2.0f / (float)m_viewportHeight;
        return proj;
    }

    void Camera::endFrame() {
        m_prevViewProjectionMatrix = getJitteredProjectionMatrix() * getViewMatrix();
        m_frameIndex++;
    }
}



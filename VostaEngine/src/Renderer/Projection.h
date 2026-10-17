#pragma once

// Every camera/light projection in the engine is built here, because the engine
// renders reverse-Z in a 0..1 clip space (glClipControl(GL_ZERO_TO_ONE)): the
// near plane lands on z_ndc = 1, the far plane on 0, the depth buffer clears to
// 0.0 and the depth test keeps the greater value.
//
// GLM's builders emit the conventional -1..1 range, so each wrapper calls them
// and then rewrites the z row. x/y are untouched: proj[2][3] keeps its -1 (the
// perspective divide is unchanged), only the z numerator and constant move.

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace ve {

    // Near -> z_ndc 1, far -> 0.
    // GLM emits proj[2][2] = -(f+n)/(f-n) and proj[3][2] = -2fn/(f-n); the target
    // is n/(f-n) and fn/(f-n).
    inline glm::mat4 makePerspective(float fovYRadians, float aspect, float nearPlane, float farPlane) {
        glm::mat4 p = glm::perspective(fovYRadians, aspect, nearPlane, farPlane);
        p[2][2] = -(p[2][2] + 1.0f) * 0.5f;
        p[3][2] = -p[3][2] * 0.5f;
        return p;
    }

    // Orthographic counterpart. w is 1 here, so the z row moves differently:
    // GLM emits -2/(f-n) and -(f+n)/(f-n) and the target is 1/(f-n) and f/(f-n).
    inline glm::mat4 makeOrtho(float left, float right, float bottom, float top,
                               float nearPlane, float farPlane) {
        glm::mat4 p = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
        p[2][2] = -p[2][2] * 0.5f;
        p[3][2] = (1.0f - p[3][2]) * 0.5f;
        return p;
    }

    // Inverse of makePerspective: the z row carries n/(f-n) and fn/(f-n), so
    // f = B/A and n = B/(1+A). The shadow split scheme sizes its slices off the
    // camera's near plane, and gets it back from the matrix it was handed.
    inline float perspectiveNear(const glm::mat4& projection) {
        return projection[3][2] / (1.0f + projection[2][2]);
    }

    inline float perspectiveFar(const glm::mat4& projection) {
        return projection[3][2] / projection[2][2];
    }

}

#include "vepch.h"
#include "OpenGLRendererAPI.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
namespace ve {

    void OpenGLRendererAPI::init(){
        glEnable(GL_BLEND);

        // Reverse-Z. glClipControl must be the first of these: it retargets the
        // whole context to a 0..1 clip depth, which every projection in
        // Renderer/Projection.h is built for, and every depth read in a shader
        // assumes. The depth buffer then clears to the far plane (0) and the
        // nearer surface wins with GREATER.
        glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
        glClearDepth(0.0);
        glDepthFunc(GL_GREATER);
    }
    void OpenGLRendererAPI::setClearColor(const glm::vec4& color) {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::clearInt(int32_t value) {
        // glClearColor is unspecified for integer color buffers; the spec-correct
        // path is glClearBufferiv. Depth is a separate attachment, so clear it
        // with the ordinary glClear.
        glClearBufferiv(GL_COLOR, 0, &value);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        //VE_CORE_SUCCESS_PRINT("glViewport called: %d %d %d %d", x, y, width, height);
        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::setScissor(int32_t x, int32_t y, int32_t width, int32_t height) {
        glScissor(x, y, width, height);
    }

    void OpenGLRendererAPI::setScissorTest(bool enabled) {
        if (enabled) {
            glEnable(GL_SCISSOR_TEST);
        }
        else {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    void OpenGLRendererAPI::drawIndexed(const Ref<VertexArray>& vertexArray) {
        vertexArray->bind();
        glDrawElements(GL_TRIANGLES, vertexArray->getIndexBuffer()->getCount(), GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t firstIndex) {
        vertexArray->bind();
        // firstIndex is an offset in indices, so scale it to bytes for the pointer offset.
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex) * sizeof(uint32_t)));
    }

    void OpenGLRendererAPI::setDepthTesting(bool enabled) {
        if (enabled) {
            glEnable(GL_DEPTH_TEST);
        }
        else {
            glDisable(GL_DEPTH_TEST);
        }

    }

    void OpenGLRendererAPI::setCursorVisible(bool visible) {
        if (visible) {
            glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else {
            glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    void OpenGLRendererAPI::drawArrays(uint32_t count) {
        glDrawArrays(GL_TRIANGLES, 0, count);
    }

    void OpenGLRendererAPI::setDepthFunc(DepthFunc func) {
        switch (func) {
        case DepthFunc::GREATER:
            glDepthFunc(GL_GREATER);
            break;
        case DepthFunc::GEQUAL:
            glDepthFunc(GL_GEQUAL);
            break;
        }
    }

    void OpenGLRendererAPI::setDepthMask(bool enabled){

        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
    }

    void OpenGLRendererAPI::setBlend(bool enabled) {
        if (enabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
    }

    void OpenGLRendererAPI::setBlendFunc(BlendFunc src, BlendFunc dst) {
        static const GLenum table[] = {
            GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
        };
        glBlendFunc(table[static_cast<int>(src)], table[static_cast<int>(dst)]);
    }

    void OpenGLRendererAPI::setWireframe(bool enabled) {
        glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }


    RendererAPI* RendererAPI::create() {
        return new OpenGLRendererAPI();
    }

}
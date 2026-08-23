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
        
    }
    void OpenGLRendererAPI::setClearColor(const glm::vec4& color) {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        //VE_CORE_SUCCESS_PRINT("glViewport called: %d %d %d %d", x, y, width, height);
        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::drawIndexed(const Ref<VertexArray>& vertexArray) {
        vertexArray->bind();
        glDrawElements(GL_TRIANGLES, vertexArray->getIndexBuffer()->getCount(), GL_UNSIGNED_INT, nullptr);
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
        case DepthFunc::LESS:
            glDepthFunc(GL_LESS);
            break;
        case DepthFunc::LEQUAL:
            glDepthFunc(GL_LEQUAL);
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


    RendererAPI* RendererAPI::create() {
        return new OpenGLRendererAPI();
    }

}
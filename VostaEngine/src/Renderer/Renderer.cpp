#include "vepch.h"
#include "Renderer.h"
#include "RenderCommand.h"
#include "Shader.h"

#include "Platform/OpenGL/OpenGLShader.h"

namespace ve {

    Renderer::SceneData Renderer::s_SceneData;

    void Renderer::beginScene(const glm::mat4& viewProjectionMatrix) {
        s_SceneData.viewProjectionMatrix = viewProjectionMatrix;

    }

    void Renderer::endScene() {
    }

    void Renderer::submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform) {
        shader->bind();
        // HACK: This breaks cross-platform compatibility because it bypasses the Shader abstraction and hardcodes a dependency on OpenGLShader.
        std::dynamic_pointer_cast<OpenGLShader>(shader)->uploadUniformMat4("u_ViewProjection", s_SceneData.viewProjectionMatrix);
        std::dynamic_pointer_cast<OpenGLShader>(shader)->uploadUniformMat4("u_Transform", transform);
        RenderCommand::drawIndexed(vertexArray);
    }

}
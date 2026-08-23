#pragma once

#include <memory>
#include <glm.hpp>
#include "Buffer.h"
#include "VertexArray.h"

namespace ve {

    class Shader;

    class VE_API Renderer {
    public:
        static void beginScene(const glm::mat4& viewProjectionMatrix);
        static void endScene();
        static void submit(const Ref<Shader>& shader,
            const Ref<VertexArray>& vertexArray,
            const glm::mat4& transform);

    private:
        struct SceneData {
            glm::mat4 viewProjectionMatrix;
        };
        static SceneData s_SceneData;
    };

}
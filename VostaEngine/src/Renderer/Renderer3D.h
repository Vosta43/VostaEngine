#pragma once
#include "Core/Core.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Shader.h"
#include "Core/AssetHandle.h"
#include "Renderer/Texture.h"
#include <glm.hpp>
#include <vector>

namespace ve {

    class VE_API Renderer3D {
    public:
        static void init();
        static void shutdown();

        static void beginScene(const glm::mat4& viewProjection);
        static void drawMesh(const glm::mat4& transform, AssetHandle meshHandle);
        static void drawPickingMesh(const glm::mat4& transform, AssetHandle meshHandle, uint32_t entityID);
        static void endScene();
        static void flush();

        static void beginPickingScene(const glm::mat4& viewProjection);
        static void endPickingScene();
        static void flushPicking();

        static void initSkybox();
        static void drawSkybox(const glm::mat4& view, const glm::mat4& projection);
        static void setSkyboxTexture(Ref<TextureCubeMap> texture);
        static void shutdownSkybox();
        static Ref<VertexArray> getSkyboxVAO() { return s_skyboxVAO; }

    private:
        struct MeshDrawCommand {
            glm::mat4 transform;
            AssetHandle meshHandle;
        };
        struct PickingDrawCommand {
            glm::mat4 transform;
            AssetHandle meshHandle;
            uint32_t entityID;
        };
        static std::vector<MeshDrawCommand> s_drawCommands;
        static std::vector<PickingDrawCommand> s_pickingCommands;
        static Ref<Shader> s_shader;
        static Ref<Shader> s_pickingShader;
        static glm::mat4 s_viewProjection;

        static Ref<Shader> s_skyboxShader;
        static Ref<VertexArray> s_skyboxVAO;
        static Ref<TextureCubeMap> s_skyboxTexture;
    };

}
#include "vepch.h"
#include "Renderer3D.h"
#include "Core/ResourceManager.h"
#include "Core/Application.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RendererAPI.h"
#include "Renderer/Buffer.h"
#include "Renderer/Material.h"
#include "Renderer/SingleMaterial.h"

namespace ve {

    std::vector<Renderer3D::MeshDrawCommand> Renderer3D::s_drawCommands;
    std::vector<Renderer3D::PickingDrawCommand> Renderer3D::s_pickingCommands;
    Ref<Shader> Renderer3D::s_shader;
    Ref<Shader> Renderer3D::s_pickingShader;
    glm::mat4 Renderer3D::s_viewProjection(1.0f);

    void Renderer3D::init() {
        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("VostaEngine/resources/shaders/3d.glsl");
        s_shader = shaderLib.get("3d");
        shaderLib.load("VostaEngine/resources/shaders/Picking3D.glsl");
        s_pickingShader = shaderLib.get("Picking3D");

        initSkybox();
    }

    void Renderer3D::shutdown() {
        s_shader = nullptr;
    }

    void Renderer3D::beginScene(const glm::mat4& viewProjection) {

        s_viewProjection = viewProjection;
        s_drawCommands.clear();
    }

    void Renderer3D::beginPickingScene(const glm::mat4& viewProjection) {

        s_viewProjection = viewProjection;
        s_pickingCommands.clear();
    }

    void Renderer3D::drawMesh(const glm::mat4& transform, AssetHandle meshHandle) {
        s_drawCommands.push_back({ transform, meshHandle });
    }

    void Renderer3D::drawPickingMesh(const glm::mat4& transform, AssetHandle meshHandle, uint32_t entityID) {
        s_pickingCommands.push_back({ transform, meshHandle, entityID });
    }


    void Renderer3D::endScene() {
        RenderCommand::setDepthTesting(true);
        flush();
    }

    void Renderer3D::endPickingScene() {
        flushPicking();
    }

    void Renderer3D::flush() {
        if (!s_shader) return;
        s_shader->bind();
        s_shader->setMat4("u_ViewProjection", s_viewProjection);

        for (const auto& cmd : s_drawCommands) {
            Ref<StaticMesh> mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
            if (!mesh) continue;

            s_shader->setMat4("u_Transform", cmd.transform);
            mesh->bind();

            const auto& submeshes = mesh->getSubmeshes();

            if (submeshes.empty()) {
                // No sub-meshes: draw the entire mesh with no material binding.
                RenderCommand::drawIndexed(mesh->getVertexArray());
                continue;
            }

            for (const auto& sub : submeshes) {
                // Bind the albedo map from the sub-mesh's material, if one exists.
                if (sub.materialHandle.isValid()) {
                    auto material = std::dynamic_pointer_cast<SingleMaterial>(ResourceManager::get<Material>(sub.materialHandle));
                    if (material && material->albedoMapHandle.isValid()) {
                        Ref<Texture2D> texture = ResourceManager::get<Texture2D>(material->albedoMapHandle);
                        if (texture) {
                            texture->bind(0);
                        }
                    }
                }

                mesh->draw(sub.firstIndex, sub.indexCount);
            }
        }
    }


    void Renderer3D::flushPicking() {
        if (!s_pickingShader) return;
        s_pickingShader->bind();
        s_pickingShader->setMat4("u_ViewProjection", s_viewProjection);

        for (const auto& cmd : s_pickingCommands) {
            Ref<StaticMesh> mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
            if (!mesh) continue;

            float idAsFloat;
            memcpy(&idAsFloat, &cmd.entityID, sizeof(float));
            s_pickingShader->setFloat4("u_EntityID", glm::vec4(idAsFloat, 0.0f, 0.0f, 1.0f));

            s_pickingShader->setMat4("u_Transform", cmd.transform);
            mesh->bind();
            RenderCommand::drawIndexed(mesh->getVertexArray());
        }
    }

    Ref<Shader> Renderer3D::s_skyboxShader;
    Ref<VertexArray> Renderer3D::s_skyboxVAO;
    Ref<TextureCubeMap> Renderer3D::s_skyboxTexture;

    // Unit cube for skybox (no index reuse across faces needed for a cube map)
    static const float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    void Renderer3D::initSkybox() {
        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("VostaEngine/resources/shaders/Skybox.glsl");
        s_skyboxShader = shaderLib.get("Skybox");

        Ref<VertexBuffer> vbo = VertexBuffer::create(
            const_cast<float*>(skyboxVertices), sizeof(skyboxVertices));
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Position" }
            });

        s_skyboxVAO = VertexArray::create();
        s_skyboxVAO->addVertexBuffer(vbo);
        // No index buffer — 36 unique vertices, draw with glDrawArrays
    }

    void Renderer3D::shutdownSkybox() {
        s_skyboxShader.reset();
        s_skyboxVAO.reset();
        s_skyboxTexture.reset();
    }

    void Renderer3D::drawSkybox(const glm::mat4& view, const glm::mat4& projection) {
        if (!s_skyboxShader || !s_skyboxTexture) return;

        glm::mat4 skyView = glm::mat4(glm::mat3(view));
        glm::mat4 vp = projection * skyView;

        RenderCommand::setDepthFunc(RendererAPI::DepthFunc::LEQUAL);
        s_skyboxShader->bind();
        s_skyboxShader->setMat4("u_ViewProjection", vp);
        s_skyboxTexture->bind(0);
        s_skyboxVAO->bind();
        RenderCommand::drawArrays(36);
        RenderCommand::setDepthFunc(RendererAPI::DepthFunc::LESS);
    }

    void Renderer3D::setSkyboxTexture(Ref<TextureCubeMap> texture) {
        s_skyboxTexture = texture;
    }


}
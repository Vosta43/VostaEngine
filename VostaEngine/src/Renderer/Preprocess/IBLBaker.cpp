#include "vepch.h"
#include "IBLBaker.h"
#include "Renderer/Shader.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Core/Application.h"
#include "Core/Log.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace ve {

// Camera parameters for the six cubemap faces (OpenGL convention).
// Camera sits at origin, looking outward along each axis.
static const struct { glm::vec3 target; glm::vec3 up; } s_faceParams[6] = {
    { glm::vec3( 1,  0,  0), glm::vec3( 0, -1,  0) },  // +X
    { glm::vec3(-1,  0,  0), glm::vec3( 0, -1,  0) },  // -X
    { glm::vec3( 0,  1,  0), glm::vec3( 0,  0,  1) },  // +Y
    { glm::vec3( 0, -1,  0), glm::vec3( 0,  0, -1) },  // -Y
    { glm::vec3( 0,  0,  1), glm::vec3( 0, -1,  0) },  // +Z
    { glm::vec3( 0,  0, -1), glm::vec3( 0, -1,  0) },  // -Z
};

Ref<TextureCubeMap> IBLBaker::bakeIrradianceMap(
    const Ref<TextureCubeMap>& inputCubemap) {

    if (!inputCubemap) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: input cubemap is null");
        return nullptr;
    }

    auto& shaderLib = Application::get().getShaderLibrary();
    shaderLib.load("VostaEngine/resources/shaders/irradianceMap.glsl");
    Ref<Shader> shader = shaderLib.get("irradianceMap");
    if (!shader) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: failed to load irradianceMap shader");
        return nullptr;
    }

    // ---- Create output cubemap (32x32 per face, RGBA16F, 1 mip) ----
    const uint32_t faceSize = 32;
    Ref<TextureCubeMap> outputCubemap = TextureCubeMap::create(faceSize);
    if (!outputCubemap) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: failed to create output cubemap");
        return nullptr;
    }

    // ---- Create temporary FBO through the engine abstraction ----
    // Framebuffer::create(w, h) creates an RGBA8 color attachment + depth/stencil.
    // The color attachment will be replaced by each cubemap face during the loop.
    Ref<Framebuffer> tempFBO = Framebuffer::create(faceSize, faceSize);

    // ---- Capture projection (90-degree FOV for exact cube-face coverage) ----
    glm::mat4 captureProj = glm::perspective(
        glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    // ---- Bind FBO, shader, and input texture ----
    tempFBO->bind();  // also sets viewport to (0, 0, faceSize, faceSize)
    shader->bind();
    inputCubemap->bind(0);
    shader->setInt("u_InputCubemap", 0);

    // ---- Render to each cubemap face ----
    for (int face = 0; face < 6; ++face) {
        // Attach the current face of the output cubemap as the color target
        tempFBO->attachCubemapFace(0, face, outputCubemap, 0);

        // Build the view-projection matrix for this face
        glm::mat4 captureView = glm::lookAt(
            glm::vec3(0.0f),
            s_faceParams[face].target,
            s_faceParams[face].up);
        shader->setMat4("u_ViewProjection", captureProj * captureView);

        // Clear and draw the unit cube (36 vertices, position = sample direction)
        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        Renderer3D::getSkyboxVAO()->bind();
        RenderCommand::drawArrays(36);
    }

    // ---- Unbind ----
    shader->unbind();
    tempFBO->unbind();

    VE_CORE_SUCCESS_PRINT("IBLBaker: irradiance map baked (%ux%u per face)",
        faceSize, faceSize);

    return outputCubemap;
}

Ref<TextureCubeMap> IBLBaker::bakePrefilteredEnvMap(
    const Ref<TextureCubeMap>& inputCubemap) {

    if (!inputCubemap) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: input cubemap is null (prefilter)");
        return nullptr;
    }

    auto& shaderLib = Application::get().getShaderLibrary();
    shaderLib.load("VostaEngine/resources/shaders/prefilterEnvMap.glsl");
    Ref<Shader> shader = shaderLib.get("prefilterEnvMap");
    if (!shader) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: failed to load prefilterEnvMap shader");
        return nullptr;
    }

    const uint32_t baseFaceSize = 128;
    const uint32_t maxMipLevels = 5;

    Ref<TextureCubeMap> outputCubemap = TextureCubeMap::create(baseFaceSize, maxMipLevels);
    if (!outputCubemap) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: failed to create prefiltered cubemap");
        return nullptr;
    }

    glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    for (uint32_t mip = 0; mip < maxMipLevels; ++mip) {
        uint32_t faceSize = baseFaceSize >> mip;
        float roughness = (float)mip / (float)(maxMipLevels - 1);

        Ref<Framebuffer> tempFBO = Framebuffer::create(faceSize, faceSize);
        tempFBO->bind();
        shader->bind();
        inputCubemap->bind(0);
        shader->setInt("u_InputCubemap", 0);
        shader->setFloat("u_Roughness", roughness);

        for (int face = 0; face < 6; ++face) {
            tempFBO->attachCubemapFace(0, face, outputCubemap, mip);

            glm::mat4 captureView = glm::lookAt(
                glm::vec3(0.0f),
                s_faceParams[face].target,
                s_faceParams[face].up);
            shader->setMat4("u_ViewProjection", captureProj * captureView);

            RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            RenderCommand::clear();

            Renderer3D::getSkyboxVAO()->bind();
            RenderCommand::drawArrays(36);
        }

        shader->unbind();
        tempFBO->unbind();
    }

    VE_CORE_SUCCESS_PRINT("IBLBaker: prefiltered env map baked (%u faces, %u mips)",
        6, maxMipLevels);

    return outputCubemap;
}

Ref<Texture2D> IBLBaker::bakeBRDFLUT() {
    auto& shaderLib = Application::get().getShaderLibrary();
    shaderLib.load("VostaEngine/resources/shaders/brdfLut.glsl");
    Ref<Shader> shader = shaderLib.get("brdfLut");
    if (!shader) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: failed to load brdfLut shader");
        return nullptr;
    }

    const uint32_t lutSize = 512;

    Ref<Texture2D> brdfLUT = Texture2D::create(lutSize, lutSize, TextureFormat::RG16F);
    if (!brdfLUT) {
        VE_CORE_ERROR_PRINT("%s", "IBLBaker: failed to create BRDF LUT texture");
        return nullptr;
    }

    // Fullscreen quad (NDC positions)
    float vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

    Ref<VertexArray> fullscreenQuad = VertexArray::create();
    auto vb = VertexBuffer::create(vertices, sizeof(vertices));
    BufferLayout layout = {
        { ShaderDataType::Float2, "a_Position" }
    };
    vb->setLayout(layout);
    fullscreenQuad->addVertexBuffer(vb);
    auto ib = IndexBuffer::create(indices, 6);
    fullscreenQuad->setIndexBuffer(ib);

    Ref<Framebuffer> tempFBO = Framebuffer::create(lutSize, lutSize);
    tempFBO->attachColorTexture(0, brdfLUT);
    tempFBO->bind();

    RenderCommand::setDepthTesting(false);
    RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    RenderCommand::clear();

    shader->bind();
    fullscreenQuad->bind();
    RenderCommand::drawIndexed(fullscreenQuad);
    fullscreenQuad->unbind();
    shader->unbind();

    RenderCommand::setDepthTesting(true);

    tempFBO->unbind();

    VE_CORE_SUCCESS_PRINT("IBLBaker: BRDF LUT baked (%ux%u RG16F)", lutSize, lutSize);

    return brdfLUT;
}

}  // namespace ve

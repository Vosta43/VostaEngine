#include "vepch.h"
#include "AtmosphereSkyBaker.h"

#include "Renderer/Shader.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/VertexArray.h"
#include "Core/Application.h"
#include "Core/Log.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace ve {

// Camera parameters for the six cubemap faces (OpenGL convention).
static const struct { glm::vec3 target; glm::vec3 up; } s_skyFaceParams[6] = {
    { glm::vec3( 1,  0,  0), glm::vec3( 0, -1,  0) },  // +X
    { glm::vec3(-1,  0,  0), glm::vec3( 0, -1,  0) },  // -X
    { glm::vec3( 0,  1,  0), glm::vec3( 0,  0,  1) },  // +Y
    { glm::vec3( 0, -1,  0), glm::vec3( 0,  0, -1) },  // -Y
    { glm::vec3( 0,  0,  1), glm::vec3( 0, -1,  0) },  // +Z
    { glm::vec3( 0,  0, -1), glm::vec3( 0, -1,  0) },  // -Z
};

Ref<TextureCubeMap> AtmosphereSkyBaker::bakeSkyCubemap(
    const AtmosphereParams& params,
    const Ref<Texture2D>& transmittance,
    const Ref<Texture3D>& scattering,
    const Ref<Texture3D>& mieScattering,
    const Ref<Texture3D>& multipleScattering,
    const glm::vec3& planetRelOrigin,
    uint32_t faceSize) {

    if (!transmittance || !scattering || !mieScattering || !multipleScattering) {
        VE_CORE_ERROR_PRINT("%s", "AtmosphereSkyBaker: atmosphere LUTs not ready");
        return nullptr;
    }

    auto& shaderLib = Application::get().getShaderLibrary();
    shaderLib.load("SandBox/assets/shaders/atmosphereSky.glsl");
    Ref<Shader> shader = shaderLib.get("atmosphereSky");
    if (!shader) {
        VE_CORE_ERROR_PRINT("%s", "AtmosphereSkyBaker: failed to load atmosphereSky shader");
        return nullptr;
    }

    Ref<TextureCubeMap> outputCubemap = TextureCubeMap::create(faceSize);
    if (!outputCubemap) {
        VE_CORE_ERROR_PRINT("%s", "AtmosphereSkyBaker: failed to create output cubemap");
        return nullptr;
    }

    Ref<Framebuffer> tempFBO = Framebuffer::create(faceSize, faceSize);
    if (!tempFBO) {
        VE_CORE_ERROR_PRINT("%s", "AtmosphereSkyBaker: failed to create temp FBO");
        return nullptr;
    }

    glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    tempFBO->bind();
    shader->bind();

    shader->setFloat3("u_PlanetRelOrigin", planetRelOrigin);
    shader->setFloat3("u_SunDirection", glm::normalize(params.sunDirection));
    shader->setFloat("u_SunIntensity", params.sunIntensity);
    shader->setFloat("u_MiePhaseG", params.miePhaseG);
    shader->setFloat("u_PlanetRadius", params.planetRadius);
    shader->setFloat("u_AtmosphereHeight", params.atmosphereHeight);
    shader->setFloat("u_MultipleScatteringStrength", params.multipleScattering);

    shader->setTexture("u_TransmittanceLUT", transmittance, 0);
    shader->setTexture3D("u_ScatteringLUT", scattering, 1);
    shader->setTexture3D("u_MieScatteringLUT", mieScattering, 2);
    shader->setTexture3D("u_MultipleScatteringLUT", multipleScattering, 3);

    for (int face = 0; face < 6; ++face) {
        tempFBO->attachCubemapFace(0, face, outputCubemap, 0);

        glm::mat4 captureView = glm::lookAt(
            glm::vec3(0.0f),
            s_skyFaceParams[face].target,
            s_skyFaceParams[face].up);
        shader->setMat4("u_ViewProjection", captureProj * captureView);

        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        Renderer3D::getSkyboxVAO()->bind();
        RenderCommand::drawArrays(36);
    }

    shader->unbind();
    tempFBO->unbind();

    VE_CORE_SUCCESS_PRINT("AtmosphereSkyBaker: sky cubemap baked (%ux%u per face)",
        faceSize, faceSize);

    return outputCubemap;
}

}  // namespace ve

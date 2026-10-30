#include "vepch.h"
#include "AtmosphereSkyBaker.h"

#include "Renderer/Shader.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Projection.h"
#include "Renderer/VertexArray.h"
#include "Renderer/SpaceBackgroundUniforms.h"
#include "Core/Application.h"
#include "Core/Log.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <algorithm>

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

namespace {

// One code path for every sky bake. A null `atmosphere` renders the space
// background alone (the shader's computeAtmosphereLUT returns black when the
// planet radius and atmosphere height are both zero); a null `space` renders the
// atmosphere alone.
Ref<TextureCubeMap> renderSkyCube(
    const AtmosphereParams* atmosphere,
    const Ref<Texture2D>& transmittance,
    const Ref<Texture3D>& scattering,
    const Ref<Texture3D>& mieScattering,
    const Ref<Texture3D>& multipleScattering,
    const SpaceBackgroundParams* space,
    const glm::vec3& planetRelOrigin,
    uint32_t faceSize) {

    auto& shaderLib = Application::get().getShaderLibrary();
    shaderLib.load("VostaEngine/resources/shaders/atmosphereSky.glsl");
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

    const glm::mat4 captureProj = makePerspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    // With no atmosphere the planet radius stays 0, which is the shader's own
    // "there is no planet" test: no LUT is ever sampled, no ground occludes the
    // sky, and the space background passes through unattenuated.
    const float planetRadius     = atmosphere ? atmosphere->planetRadius : 0.0f;
    const float atmosphereHeight = atmosphere ? atmosphere->atmosphereHeight : 0.0f;

    tempFBO->bind();
    shader->bind();

    shader->setFloat3("u_PlanetRelOrigin", planetRelOrigin);
    shader->setFloat3("u_SunDirection",
        atmosphere ? glm::normalize(atmosphere->sunDirection) : glm::vec3(0.0f, 1.0f, 0.0f));
    shader->setFloat3("u_SunIntensity",
        atmosphere ? atmosphere->sunIntensity * atmosphere->resolveSunColor() : glm::vec3(0.0f));
    shader->setFloat("u_MiePhaseG", atmosphere ? atmosphere->miePhaseG : 0.0f);
    shader->setFloat("u_PlanetRadius", planetRadius);
    shader->setFloat("u_AtmosphereHeight", atmosphereHeight);
    shader->setFloat("u_MultipleScatteringStrength",
        atmosphere ? atmosphere->multipleScattering : 0.0f);
    shader->setFloat("u_CamAltitude", atmosphere
        ? std::max(glm::length(planetRelOrigin) - planetRadius, 0.0f)
        : 0.0f);

    if (transmittance) {
        shader->setTexture("u_TransmittanceLUT", transmittance, 0);
    }
    if (scattering) {
        shader->setTexture3D("u_ScatteringLUT", scattering, 1);
    }
    if (mieScattering) {
        shader->setTexture3D("u_MieScatteringLUT", mieScattering, 2);
    }
    if (multipleScattering) {
        shader->setTexture3D("u_MultipleScatteringLUT", multipleScattering, 3);
    }

    // The star PSF is clamped to one pixel of whatever target it is drawn into,
    // so at 64-128 px per face each star becomes a smooth blob instead of an
    // aliased point. Its total flux is preserved, which is all the irradiance
    // convolution integrates.
    const float pixelAngle = 2.0f / (captureProj[1][1] * (float)faceSize);
    uploadSpaceBackground(*shader, space, pixelAngle);

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

    return outputCubemap;
}

}  // namespace

Ref<TextureCubeMap> AtmosphereSkyBaker::bakeSkyCubemap(
    const AtmosphereParams& params,
    const Ref<Texture2D>& transmittance,
    const Ref<Texture3D>& scattering,
    const Ref<Texture3D>& mieScattering,
    const Ref<Texture3D>& multipleScattering,
    const SpaceBackgroundParams* space,
    const glm::vec3& planetRelOrigin,
    uint32_t faceSize) {

    if (!transmittance || !scattering || !mieScattering || !multipleScattering) {
        VE_CORE_ERROR_PRINT("%s", "AtmosphereSkyBaker: atmosphere LUTs not ready");
        return nullptr;
    }

    Ref<TextureCubeMap> cubemap = renderSkyCube(
        &params, transmittance, scattering, mieScattering, multipleScattering,
        space, planetRelOrigin, faceSize);
    if (!cubemap) {
        return nullptr;
    }

    VE_CORE_SUCCESS_PRINT("AtmosphereSkyBaker: sky cubemap baked (%ux%u per face)%s",
        faceSize, faceSize, space ? " with space background" : "");
    return cubemap;
}

Ref<TextureCubeMap> AtmosphereSkyBaker::bakeSpaceSkyCubemap(
    const SpaceBackgroundParams& space,
    uint32_t faceSize) {

    Ref<TextureCubeMap> cubemap = renderSkyCube(
        nullptr, nullptr, nullptr, nullptr, nullptr,
        &space, glm::vec3(0.0f), faceSize);
    if (!cubemap) {
        return nullptr;
    }

    VE_CORE_SUCCESS_PRINT("AtmosphereSkyBaker: space sky cubemap baked (%ux%u per face)",
        faceSize, faceSize);
    return cubemap;
}

}  // namespace ve

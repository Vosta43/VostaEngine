#include "vepch.h"
#include "SceneViewRenderer.h"

#include "Renderer/RenderPipeline.h"
#include "Renderer/Shadow/ShadowCascadeCalculator.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Shader.h"
#include "Renderer/Preprocess/BakeService.h"
#include "Core/Deltatime.h"
#include "Core/ResourceManager.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"

namespace ve {

SceneViewRenderer::SceneViewRenderer(const Ref<Scene>& scene)
    : m_scene(scene)
    , m_sceneRenderer(CreateRef<SceneRenderer>(scene))
{
}

SceneViewRenderer::~SceneViewRenderer() = default;

void SceneViewRenderer::render(Camera& camera, const Ref<RenderPipeline>& pipeline,
                               const Ref<Framebuffer>& target)
{
    BakeService::get().update();

    uint32_t fbW = target->getWidth();
    uint32_t fbH = target->getHeight();

    pipeline->resize(fbW, fbH);

    RenderContext ctx;
    ctx.viewPortWidth  = fbW;
    ctx.viewPortHeight = fbH;
    ctx.viewMatrix     = camera.getViewMatrix();
    ctx.projMatrix     = camera.getJitteredProjectionMatrix();
    ctx.prevViewProjMatrix = camera.getPrevViewProjectionMatrix();
    ctx.cameraPosition = camera.getPosition();
    ctx.totalTime      = DeltaTime::get().getCurrentTime();
    ctx.frameIndex     = camera.getFrameIndex();
    ctx.wireframe      = m_wireframe;
    ctx.groundGrid     = m_groundGrid;
    ctx.projMatrixNoJitter = camera.getProjectionMatrix();

    m_scene->setCameraMatrices(ctx.viewMatrix, ctx.projMatrix);
    m_scene->setCameraPosition(ctx.cameraPosition);

    ctx.drawMeshCommands.clear();
    ctx.drawLightCommands.clear();

    m_sceneRenderer->collectAllMesh(ctx);
    m_sceneRenderer->collectAllLight(ctx);
    m_sceneRenderer->collectAllSprites(ctx);

    // Skybox + IBL
    Ref<TextureCubeMap> cubemap;
    auto skyView = m_scene->getRegistry().view<SkyBoxComponent>();
    if (!skyView.empty()) {
        auto entity = *skyView.begin();
        auto& skyComp = m_scene->getComponent<SkyBoxComponent>(entity);
        cubemap = ResourceManager::get<TextureCubeMap>(skyComp.textureCubeMapHandle);
        BakeService::get().setSkybox(cubemap);
    }
    ctx.skyboxTexture     = cubemap;
    ctx.prefilteredEnvMap = BakeService::get().getPrefilteredEnvMap();
    ctx.brdfLUT           = BakeService::get().getBRDFLUT();

    // Atmosphere (single global entity, like the skybox) + clouds
    auto& bake = BakeService::get();
    auto atmosphereView = m_scene->getRegistry().group<TransformComponent, AtmosphereComponent>();
    if (!atmosphereView.empty()) {
        auto entity = *atmosphereView.begin();
        auto& transform = m_scene->getComponent<TransformComponent>(entity);
        auto& atmosphereComp = m_scene->getComponent<AtmosphereComponent>(entity);
        ctx.atmosphere = atmosphereComp.atmosphere;
        ctx.clouds = atmosphereComp.clouds;
        ctx.planetCenter = glm::vec3(transform.transform[3]);
        ctx.hasAtmosphere = true;

        ctx.transmittanceTexture = bake.getTransmittanceLUT(atmosphereComp.atmosphere);
        ctx.scatteringTexture = bake.getScatteringLUT(atmosphereComp.atmosphere);
        ctx.mieScatteringTexture = bake.getMieScatteringLUT(atmosphereComp.atmosphere);
        ctx.multipleScatteringTexture = bake.getMultipleScatteringLUT(atmosphereComp.atmosphere);

        // Fit this frame's shadow cascades. The map resolution comes from the shadow
        // framebuffers so it can't drift from the pipeline asset; the coverage
        // distance is a pipeline setting.
        {
            static const char* kCascadeFbos[ShadowCascadeCalculator::kCascades] = { "shadow0", "shadow1", "shadow2" };
            int mapRes[ShadowCascadeCalculator::kCascades] = { 1, 1, 1 };
            for (int i = 0; i < ShadowCascadeCalculator::kCascades; ++i) {
                if (auto fb = pipeline->framebuffer(kCascadeFbos[i])) {
                    mapRes[i] = (int)fb->getWidth();
                }
            }
            const float shadowDistance = pipeline->settings().getFloat("SHADOW_DISTANCE", 250.0f);
            ShadowCascades sc = ShadowCascadeCalculator::compute(
                ctx.viewMatrix, ctx.projMatrix, ctx.cameraPosition,
                glm::normalize(ctx.atmosphere.sunDirection), shadowDistance, mapRes,
                ShadowCascadeCalculator::kCascades);
            for (int i = 0; i < sc.count; ++i) {
                ctx.shadowLightVP[i]   = sc.lightVP[i];
                ctx.shadowSplitFar[i]  = sc.splitFar[i];
                ctx.shadowTexelWorld[i] = sc.texelWorld[i];
            }
            ctx.shadowCascadeCount = sc.count;
        }

        const auto& clouds = bake.getCloudTextures();
        ctx.cloudNoiseTexture  = clouds.noise;
        ctx.cloudWorleyCells   = clouds.worleyCells;
        ctx.cloudDetailTexture = clouds.detail;
        ctx.cloudDetailCells   = clouds.detailCells;
        ctx.cloudWarpTexture   = clouds.warp;
        ctx.cloudWarpCells     = clouds.warpCells;
        ctx.cloudWeatherMap    = clouds.weatherMap;
    }

    // Diffuse ambient follows the atmosphere when one is present: the analytic
    // sky is baked to a cubemap and convolved, so object lighting matches the
    // visible sky (the static-skybox IBL would go stale as the sun moves).
    if (ctx.hasAtmosphere) {
        ctx.irradianceMap = BakeService::get().getAtmosphereIrradianceMap(
            ctx.atmosphere, ctx.transmittanceTexture, ctx.scatteringTexture,
            ctx.mieScatteringTexture, ctx.multipleScatteringTexture,
            ctx.cameraPosition - ctx.planetCenter);
    }
    else {
        ctx.irradianceMap = BakeService::get().getIrradianceMap();
    }

    // The pipeline's present pass blits the tone-mapped image into the target.
    ctx.outputFrameBuffer = target;
    ctx.presentDiscardBackground = false;

    pipeline->render(ctx);
    camera.endFrame();

    // Present already cleared + filled the target; draw the component-driven
    // game content (skybox + sprites) on top of it.
    target->bind();

    if (cubemap) {
        Renderer3D::setSkyboxTexture(cubemap);
        Renderer3D::drawSkybox(ctx.viewMatrix, ctx.projMatrix);
    }

    // Sprite quads are game content drawn on top of the PBR output.
    if (!ctx.drawSpriteCommands.empty()) {
        Renderer2D::beginScene(ctx.projMatrix * ctx.viewMatrix);
        for (auto& cmd : ctx.drawSpriteCommands) {
            Renderer2D::drawQuad(cmd.transform, cmd.size, cmd.textureHandle);
        }
        Renderer2D::endScene();
    }

    target->unbind();
}

}

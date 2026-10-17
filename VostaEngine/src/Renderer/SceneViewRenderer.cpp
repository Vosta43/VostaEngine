#include "vepch.h"
#include "SceneViewRenderer.h"

#include "Renderer/RenderPipeline.h"
#include "Renderer/Shadow/ShadowCascadeCalculator.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Shader.h"
#include "Renderer/RuntimeVirtualTexture.h"
#include "Renderer/GpuReadbackSelfTest.h"
#include "Renderer/Preprocess/BakeService.h"
#include "Renderer/LayeredMaterial.h"
#include "Renderer/Material.h"
#include "Core/Deltatime.h"
#include "Core/ResourceManager.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Scene/Terrain/Terrain.h"
#include "Terrain/RuleGraphResource.h"
#include "Terrain/GlobeWeightBaker.h"

#include <algorithm>
#include <string>

namespace ve {

SceneViewRenderer::SceneViewRenderer(const Ref<Scene>& scene)
    : m_scene(scene)
    , m_sceneRenderer(CreateRef<SceneRenderer>(scene))
{
    // Adopt the tiles an earlier streamer left in this scene: the editor swaps
    // this renderer out for a play session and back, and the authored scene keeps
    // whatever window it had. Without adopting them the new streamer would read
    // them as authored tiles -- recreated-when-covered, never evicted -- so a
    // session's window would outlive the view that asked for it.
    if (m_scene) {
        for (uint32_t id : m_scene->getRegistry().group<TransformComponent, TerrainComponent>()) {
            const TerrainComponent& comp = m_scene->getComponent<TerrainComponent>(id);
            if (comp.bStreamed)
                m_streamedTileCoords.insert(TerrainDataResource::pack(comp.tileCoord));
        }
    }
}

SceneViewRenderer::~SceneViewRenderer() = default;

GlobeStats SceneViewRenderer::getGlobeStats() const {
    return GlobeStats{ (int)m_globeLod.tileCount(), m_globeLod.maxLevel(), m_globeLod.builtLastFrame() };
}

void SceneViewRenderer::render(Camera& camera, const Ref<RenderPipeline>& pipeline,
                               const Ref<Framebuffer>& target)
{
    BakeService::get().update();

    uint32_t fbW = target->getWidth();
    uint32_t fbH = target->getHeight();

    pipeline->resize(fbW, fbH);

    // The subpixel jitter exists only to feed TAA; without accumulation it would
    // just shimmer. Must be set before the projection matrices below are read.
    camera.setJitterEnabled(m_taa);

    RenderContext ctx;
    ctx.viewPortWidth  = fbW;
    ctx.viewPortHeight = fbH;
    ctx.cameraPosition = camera.getPosition();   // chart; the scene queries below read it
    ctx.projMatrix     = camera.getJitteredProjectionMatrix();
    ctx.totalTime      = DeltaTime::get().getCurrentTime();
    ctx.frameIndex     = camera.getFrameIndex();
    ctx.wireframe      = m_wireframe;
    ctx.groundGrid     = m_groundGrid;
    ctx.taaEnabled     = m_taa;
    ctx.rvtDebugView   = m_rvtDebugView;
    ctx.projMatrixNoJitter = camera.getProjectionMatrix();

    // Chart -> sphere, from the scene's atmosphere (the only thing that describes a
    // planet). The component values are copied here, before render space is
    // anchored on the sphere; the LUTs and shadow fit follow later, once the view
    // exists. With no atmosphere the frame stays flat and nothing below moves.
    auto& bake = BakeService::get();
    {
        auto atmosphereView = m_scene->getRegistry().group<TransformComponent, AtmosphereComponent>();
        if (!atmosphereView.empty()) {
            const uint32_t entity = *atmosphereView.begin();
            const auto& transform = m_scene->getComponent<TransformComponent>(entity);
            const auto& atmosphereComp = m_scene->getComponent<AtmosphereComponent>(entity);
            ctx.atmosphere = atmosphereComp.atmosphere;
            ctx.clouds = atmosphereComp.clouds;
            ctx.planetCenter = transform.position;
            ctx.hasAtmosphere = true;
            // The centre is the sphere's, valid however the radius is set; only a
            // positive radius lifts the chart off the plane.
            ctx.planetFrame.center = glm::dvec3(transform.position);
            const double radius = (double)atmosphereComp.atmosphere.planetRadius;
            if (radius > 0.0) {
                ctx.planetFrame.radius = radius;
            }
        }
    }

    // Render space is anchored at the camera -- the PLACED camera, so the view is
    // the bare orientation and every pass rebases models, lights and the planet
    // centre into it. cameraPosition stays a chart point for the scene queries.
    ctx.renderOrigin = ve::Vec3(ctx.planetFrame.place(ctx.cameraPosition));
    ctx.viewMatrix  = camera.getRenderViewMatrix();

    // Prototype planet body: the face-indexed LOD set for this frame. Needs a real
    // radius (only an atmosphere provides one); with none the frame is flat and
    // there is no globe. Runs after renderOrigin, which the render-space mapping
    // below needs. Selection only -- seams are not stitched (cracks expected).
    ctx.globeTiles.clear();
    if (m_showGlobe && !ctx.planetFrame.isFlat()) {
        // Screen-space error is measured from the non-jittered projection; the TAA
        // subpixel jitter would otherwise make the LOD flicker frame to frame.
        const float projY = ctx.projMatrixNoJitter[1][1];
        const float pixelsPerRadian = (projY > 1e-6f && ctx.viewPortHeight > 0)
            ? 0.5f * projY * (float)ctx.viewPortHeight
            : 1.0f;
        m_globeLod.update(
            ctx.planetFrame.cameraRelativeToCenter(ctx.cameraPosition),
            toRenderSpace(ctx.planetCenter, ctx.renderOrigin),
            ctx.projMatrixNoJitter * ctx.viewMatrix,
            (double)ctx.planetFrame.radius,
            pixelsPerRadian);
        for (const auto& t : m_globeLod.tiles())
            ctx.globeTiles.push_back(GlobeTileDraw{ t.mesh, t.centerLocal });
    }

    // Rule-driven globe surface: shade the tiles with the terrain's own layered
    // material in sphere (direction) mode, its weights baked from the same rule graph
    // -- so the globe is covered by the whole sphere rather than the chart's one
    // hemisphere. Without a rule graph the globe keeps the GBuffer pass's fallback.
    ctx.globeMaterial = nullptr;
    ctx.globeWeights = nullptr;
    if (!ctx.globeTiles.empty()) {
        if (TerrainSystemComponent* sys = TerrainSubsystem(*m_scene).system()) {
            auto layered = std::dynamic_pointer_cast<LayeredMaterial>(
                ResourceManager::get<Material>(sys->materialHandle));
            const int layerCount = layered
                ? (int)std::min(layered->layers.size(), (size_t)kMaxMaterialLayers) : 0;
            if (layered && layerCount > 1 && sys->ruleGraphHandle.isValid()) {
                if (auto res = ResourceManager::get<TerrainRuleGraphResource>(sys->ruleGraphHandle)) {
                    if (res->contentHash != 0) {
                        const float fieldExtent = sys->globeNoiseScale;
                        const std::string key = std::to_string(res->contentHash)
                            + "|" + std::to_string(layerCount) + "|" + std::to_string(fieldExtent);
                        if (!m_globeWeights || key != m_globeWeightsKey) {
                            m_globeWeights = bakeGlobeWeights(res->graph, layerCount,
                                kGlobeWeightWidth, kGlobeWeightHeight, fieldExtent);
                            m_globeWeightsKey = key;
                        }
                        if (m_globeWeights) {
                            ctx.globeMaterial = layered;
                            ctx.globeWeights = m_globeWeights;
                        }
                    }
                }
            }
        }
    }

    // TAA reprojects history through the previous frame's view-projection. Both
    // sides must live in the same render space, so fold this frame's origin shift
    // into the stored previous matrix: it then maps a render-space point of THIS
    // frame back to the previous frame's clip space.
    const glm::mat4 viewProj = ctx.projMatrix * ctx.viewMatrix;
    ctx.prevViewProjMatrix = m_hasPrevView
        ? m_prevViewProj * glm::translate(glm::mat4(1.0f), glm::vec3(ctx.renderOrigin - m_prevViewOrigin))
        : viewProj;
    m_prevViewProj   = viewProj;
    m_prevViewOrigin = ctx.renderOrigin;
    m_hasPrevView    = true;

    // Scene queries keep the authoritative (absolute) view and camera position:
    // gameplay scripts and terrain streaming work in world space.
    m_scene->setCameraMatrices(camera.getViewMatrix(), ctx.projMatrix);
    m_scene->setCameraPosition(ctx.cameraPosition);

    // Dynamic terrain, before the collect pass so a tile added here is built and
    // drawn on this frame rather than the next. The viewpoint is whichever camera
    // this view renders from -- the editor's fly camera in Edit, the scene camera
    // in Play -- which is what makes the window follow the view in both.
    if (TerrainSystemComponent* terrainSystem = TerrainSubsystem(*m_scene).system())
        TerrainSubsystem(*m_scene).streamUpdate(m_streamedTileCoords, ctx.cameraPosition, *terrainSystem);

    ctx.drawMeshCommands.clear();
    ctx.drawLightCommands.clear();

    m_sceneRenderer->collectAllMesh(ctx);
    m_sceneRenderer->collectAllLight(ctx);
    m_sceneRenderer->collectAllSprites(ctx);

    // Built and refreshed here, the single point both Play and the editor view go
    // through, so the two render paths cannot disagree about its content.
    if (ctx.hasTerrainBounds) {
        if (!m_rvt) m_rvt = CreateRef<RuntimeVirtualTexture>();
        if (!m_rvt->isInitialized()) m_rvt->init(RuntimeVirtualTexture::Settings{});
        m_rvt->setWorldBounds(ctx.terrainMinXZ, ctx.terrainMinXZ + ctx.terrainSizeXZ);

        // World units one screen pixel spans at unit view distance; the RVT turns
        // that into the mip level each location's distance asks for.
        const float projY = ctx.projMatrixNoJitter[1][1];
        const float worldPerPixelAtUnitDepth = (projY > 1e-6f && ctx.viewPortHeight > 0)
            ? 2.0f / (projY * (float)ctx.viewPortHeight)
            : 1e-3f;
        m_rvt->requestPages(glm::vec2(ctx.cameraPosition.x, ctx.cameraPosition.z),
                            worldPerPixelAtUnitDepth);
        ctx.rvt = m_rvt;
    }

    // TEMPORARY: exercises the image-write + async-readback path once; delete with
    // GpuReadbackSelfTest.
    if (GpuReadbackSelfTest::runOnce()) {
        RenderCommand::setViewport((uint32_t)ctx.viewPortX, (uint32_t)ctx.viewPortY,
                                   (uint32_t)ctx.viewPortWidth, (uint32_t)ctx.viewPortHeight);
    }

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

    // Atmosphere LUTs, shadow cascades and cloud textures. The atmosphere, clouds
    // and planet centre were copied above; only the LUTs and the frame's shadow
    // fit are left, and they need the view the earlier block could not have had.
    if (ctx.hasAtmosphere) {
        ctx.transmittanceTexture = bake.getTransmittanceLUT(ctx.atmosphere);
        ctx.scatteringTexture = bake.getScatteringLUT(ctx.atmosphere);
        ctx.mieScatteringTexture = bake.getMieScatteringLUT(ctx.atmosphere);
        ctx.multipleScatteringTexture = bake.getMultipleScatteringLUT(ctx.atmosphere);

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

    // Optional sun god-rays, from a standalone component. The pass itself no-ops
    // when there is no atmosphere (no sun, so no shadow cascades to sample).
    auto volView = m_scene->getRegistry().view<VolumetricLightComponent>();
    if (!volView.empty()) {
        auto& vol = m_scene->getComponent<VolumetricLightComponent>(*volView.begin());
        if (vol.enabled) {
            ctx.volumetric = vol.params;
            ctx.hasVolumetricLight = true;
        }
    }

    // Diffuse ambient follows the atmosphere when one is present: the analytic
    // sky is baked to a cubemap and convolved, so object lighting matches the
    // visible sky (the static-skybox IBL would go stale as the sun moves).
    if (ctx.hasAtmosphere) {
        ctx.irradianceMap = BakeService::get().getAtmosphereIrradianceMap(
            ctx.atmosphere, ctx.transmittanceTexture, ctx.scatteringTexture,
            ctx.mieScatteringTexture, ctx.multipleScatteringTexture,
            ctx.planetFrame.cameraRelativeToCenter(ctx.cameraPosition));
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
    // game content (skybox + sprites) on top of it. The RVT debug view replaces
    // the whole image, so it suppresses this overlay.
    target->bind();

    const bool rvtDebugView = ctx.rvt != nullptr && ctx.rvtDebugView != 0;

    if (!rvtDebugView && cubemap) {
        Renderer3D::setSkyboxTexture(cubemap);
        Renderer3D::drawSkybox(ctx.viewMatrix, ctx.projMatrix);
    }

    // Sprite quads are game content drawn on top of the PBR output.
    if (!rvtDebugView && !ctx.drawSpriteCommands.empty()) {
        Renderer2D::beginScene(ctx.projMatrix * ctx.viewMatrix);
        for (auto& cmd : ctx.drawSpriteCommands) {
            Renderer2D::drawQuad(renderModelMatrix(cmd.transform, cmd.worldPosition, ctx), cmd.size, cmd.textureHandle);
        }
        Renderer2D::endScene();
    }

    target->unbind();
}

}

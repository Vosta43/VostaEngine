#pragma once

#include "Core/Core.h"
#include "Renderer/Camera.h"
#include "Renderer/FrameBuffer.h"
#include "Geometry/CubeSphereLod.h"

#include <cstdint>
#include <string>
#include <unordered_set>

namespace ve {

class Scene;
class SceneRenderer;
class RenderPipeline;
class RuntimeVirtualTexture;
class Texture2DArray;

// Globe LOD readout for the editor toolbar: what the last update() selected.
struct GlobeStats {
    int tiles    = 0;
    int maxLevel = 0;
    int built    = 0;
};

// Renders the CONTENT of a scene into a target framebuffer for one frame:
// collects meshes/lights/sprites into a RenderContext, fills skybox IBL and
// atmosphere/cloud LUTs via BakeService, runs the RenderPipeline, then
// presents the result (screen pass + skybox + sprite quads) into the target.
//
// Editor gizmos (light billboards, selection outlines) are deliberately NOT
// here — they are editor-only overlays drawn by the layer on top.
class VE_API SceneViewRenderer {
public:
    explicit SceneViewRenderer(const Ref<Scene>& scene);
    ~SceneViewRenderer();

    void render(Camera& camera, const Ref<RenderPipeline>& pipeline,
                const Ref<Framebuffer>& target);

    // Debug wireframe view: when on, meshes are rasterized as lines in the
    // GBuffer pass. Off by default.
    void setWireframe(bool wireframe) { m_wireframe = wireframe; }

    // Editor view helper: y=0 ground grid composited by the present pass. Off by
    // default.
    void setGroundGrid(bool groundGrid) { m_groundGrid = groundGrid; }

    // Temporal anti-aliasing: the accumulate pass plus the camera's subpixel
    // jitter. On by default; turning it off disables the jitter too, so the
    // image stays steady without accumulation.
    void setTAAEnabled(bool enabled) { m_taa = enabled; }

    // Runtime virtual texture debug view (0 = off; see RuntimeVirtualTexture::DebugView).
    void setRVTDebugView(int view) { m_rvtDebugView = view; }

    // Editor prototype toggle: draw a fixed-LOD cube-sphere as the planet body.
    void setShowGlobe(bool show) { m_showGlobe = show; }

    GlobeStats getGlobeStats() const;

private:
    Ref<Scene>         m_scene;
    Ref<SceneRenderer> m_sceneRenderer;
    bool               m_wireframe = false;
    bool               m_groundGrid = false;
    bool               m_taa = true;
    int                m_rvtDebugView = 0;
    bool               m_showGlobe = false;
    // The prototype globe's face-indexed LOD quad-tree (selects + caches tiles).
    CubeSphereLod      m_globeLod;
    // Last frame's jittered view-projection and the origin it was built against.
    glm::mat4          m_prevViewProj   = glm::mat4(1.0f);
    // Authoritative precision: the frame-to-frame origin shift feeds TAA
    // reprojection, and narrowing it to float32 would offset history by a ULP of
    // the absolute origin (metres at planet scale).
    ve::Vec3           m_prevViewOrigin{ 0.0 };
    bool               m_hasPrevView    = false;
    // The paged virtual texture over this scene's terrain; built lazily.
    Ref<RuntimeVirtualTexture> m_rvt;
    // The globe's sphere-domain weight atlas (baked from the terrain rule graph) and
    // the key it was baked for; rebuilt when the rule graph or layer count changes.
    Ref<Texture2DArray> m_globeWeights;
    std::string         m_globeWeightsKey;
    // Terrain tiles this renderer's streamer created, packed like
    // TerrainDataResource keys. See TerrainSubsystem::streamUpdate.
    std::unordered_set<int64_t> m_streamedTileCoords;
};

}

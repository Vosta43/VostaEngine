#pragma once

#include "Core/Core.h"
#include "Renderer/Camera.h"
#include "Renderer/FrameBuffer.h"

namespace ve {

class Scene;
class SceneRenderer;
class RenderPipeline;

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

private:
    Ref<Scene>         m_scene;
    Ref<SceneRenderer> m_sceneRenderer;
    bool               m_wireframe = false;
};

}

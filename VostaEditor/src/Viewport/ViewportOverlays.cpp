#include "Viewport/ViewportOverlays.h"

#include "SubEditors/TerrainEditor.h"

#include "Core/ResourceManager.h"
#include "Scene/Terrain/Terrain.h"

#include "imguizmo.h"

#include <gtc/type_ptr.hpp>

#include <cmath>
#include <vector>

namespace ve {

namespace {

// Project a world point into viewport pixel coordinates (y-down -- the space
// ImGui draw lists use). False when the point is behind the camera.
bool projectToViewport(const glm::mat4& viewProj, const glm::vec3& world,
                       const ImVec2& viewportPos, const ImVec2& viewportSize,
                       ImVec2& outScreen) {
    const glm::vec4 clip = viewProj * glm::vec4(world, 1.0f);
    if (clip.w <= 1e-5f)
        return false;
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    outScreen.x = viewportPos.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x;
    outScreen.y = viewportPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.y;
    return true;
}

}

void ViewportOverlays::attach() {
    auto& shaderLib = Application::get().getShaderLibrary();
    shaderLib.load("VostaEngine/resources/shaders/Picking.glsl");
    m_pickingShader = shaderLib.get("Picking");

    m_pointLightIcon = ResourceManager::store<Texture2D>("VostaEditor/resources/icons/point_light.png");
    m_cameraIcon     = ResourceManager::store<Texture2D>("VostaEditor/resources/icons/camera.png");
}

void ViewportOverlays::ensurePickTarget(uint32_t width, uint32_t height) {
    if (!m_pickingFramebuffer || m_pickingFramebuffer->getWidth() != width
        || m_pickingFramebuffer->getHeight() != height) {
        m_pickingFramebuffer = Framebuffer::create(width, height, Framebuffer::Format::Picking);
    }
}

void ViewportOverlays::drawOverlays(SceneView& view, const Ref<Scene>& scene, uint32_t& selectedEntity,
                                    const ImVec2& viewportPos, const ImVec2& viewportSize,
                                    bool mouseInViewport, bool sceneLive, bool showTerrainBrush,
                                    TerrainEditor& terrainEditor, const glm::vec2& mouseViewportPos) {
    if (!scene)
        return;

    ImGuizmo::BeginFrame();
    if (selectedEntity != UINT32_MAX) {
        auto entityID = selectedEntity;

        // The selection is a raw id with no generation, and entities the editor/AI
        // create need not carry a transform. Only drive the gizmo for a live entity
        // that actually owns a TransformComponent.
        if (scene->getEntity(entityID).getId() != 0xFFFFFFFFu &&
            scene->getRegistry().has<TransformComponent>(entityID)) {
            auto& tc = scene->getComponent<TransformComponent>(entityID);

            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

            static ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_W)) op = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) op = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) op = ImGuizmo::SCALE;

            auto& camera = view.getCamera();
            glm::mat4 camView = camera.getViewMatrix();
            glm::mat4 camProj = camera.getProjectionMatrix();
            glm::mat4 transform = tc.matrix();

            ImGuizmo::Manipulate(
                glm::value_ptr(camView), glm::value_ptr(camProj),
                op, ImGuizmo::LOCAL, glm::value_ptr(transform)
            );

            if (ImGuizmo::IsUsing()) {
                float t[3], r[3], s[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), t, r, s);
                tc.rotation = glm::quat(glm::radians(glm::vec3(r[0], r[1], r[2])));
                tc.scale = glm::vec3(s[0], s[1], s[2]);
                // The gizmo matrix is float32, so only the (small, precise) drag
                // delta is trustworthy -- fold it into the authoritative position
                // instead of overwriting it, which would quantize a planet-scale
                // tile to the float gizmo grid.
                const glm::vec3 base = glm::vec3(tc.position);
                tc.position += ve::Vec3(glm::vec3(t[0], t[1], t[2]) - base);

                // A terrain tile sits on an integer grid, so translation is snapped
                // every frame of the drag rather than on release.
                if (scene->getRegistry().has<TerrainComponent>(entityID))
                    TerrainTile(*scene, entityID).snapToGrid();
            }
        }
    }

    // Camera gizmo: the frustum wireframe of every CameraComponent, so a game
    // camera's placement and facing read at a glance. Editor overlay only -- the
    // engine draws nothing for cameras. Skipped during a play session, where the
    // viewport shows the runtime scene.
    if (!sceneLive) {
        auto cameraView = scene->getRegistry().view<CameraComponent>();
        if (!cameraView.empty()) {
            // The piloted camera IS the viewport camera, so its own frustum would
            // be drawn at the eye -- skip it.
            const bool sceneDriven = view.getCameraSource() == SceneView::CameraSource::Scene;
            uint32_t activeCam = view.getSceneCameraEntity().getId();
            if (activeCam == 0xFFFFFFFFu) activeCam = scene->getPrimaryCameraEntity().getId();

            const glm::mat4 viewProj =
                view.getCamera().getProjectionMatrix() * view.getCamera().getViewMatrix();
            const float aspect = viewportSize.x / viewportSize.y;
            ImDrawList* dl = ImGui::GetWindowDrawList();

            for (auto entity : cameraView) {
                if (sceneDriven && entity == activeCam) continue;
                if (!scene->getRegistry().has<TransformComponent>(entity)) continue;
                const auto& cam = scene->getComponent<CameraComponent>(entity);
                const auto& tf  = scene->getComponent<TransformComponent>(entity);

                // View-space corners (a GL camera looks down -Z), widened by the
                // aspect this viewport would give it.
                const float tanHalf = std::tan(glm::radians(cam.fov) * 0.5f);
                const float nH = tanHalf * cam.nearPlane, nW = nH * aspect;
                const float fH = tanHalf * cam.farPlane,  fW = fH * aspect;
                const glm::vec3 corners[8] = {
                    {-nW, -nH, -cam.nearPlane}, { nW, -nH, -cam.nearPlane},
                    { nW,  nH, -cam.nearPlane}, {-nW,  nH, -cam.nearPlane},
                    {-fW, -fH, -cam.farPlane }, { fW, -fH, -cam.farPlane },
                    { fW,  fH, -cam.farPlane }, {-fW,  fH, -cam.farPlane },
                };
                ImVec2 screen[8];
                bool ok[8];
                for (int i = 0; i < 8; ++i) {
                    const glm::vec3 world = glm::vec3(tf.matrix() * glm::vec4(corners[i], 1.0f));
                    ok[i] = projectToViewport(viewProj, world, viewportPos, viewportSize, screen[i]);
                }

                static const int edges[12][2] = {
                    {0,1},{1,2},{2,3},{3,0},   // near rect
                    {4,5},{5,6},{6,7},{7,4},   // far rect
                    {0,4},{1,5},{2,6},{3,7},   // connectors
                };
                for (const auto& e : edges) {
                    if (ok[e[0]] && ok[e[1]])
                        dl->AddLine(screen[e[0]], screen[e[1]], IM_COL32(255, 200, 60, 220), 1.5f);
                }
            }
        }
    }

    // Terrain brush cursor: a ring on the surface where a stroke would land. It
    // lies in the tangent plane so it follows a slope instead of reading as a flat
    // top-down disc, and its radius matches the world-space paint ball.
    if (showTerrainBrush && mouseInViewport) {
        auto& camera = view.getCamera();
        const glm::vec2 size(viewportSize.x, viewportSize.y);
        glm::vec3 hit, hitNormal(0.0f, 1.0f, 0.0f);
        if (terrainEditor.hoverPoint(camera.getViewMatrix(), camera.getProjectionMatrix(),
                mouseViewportPos, size, *scene, hit, hitNormal)) {
            const glm::mat4 viewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
            const float r = terrainEditor.radius();
            const int segs = 72;

            // Tangent basis around the normal; the fallback avoids a degenerate
            // cross when the surface faces straight up.
            const glm::vec3 up = std::abs(hitNormal.y) < 0.99f
                ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            const glm::vec3 T = glm::normalize(glm::cross(up, hitNormal));
            const glm::vec3 B = glm::cross(hitNormal, T);

            std::vector<ImVec2> ring;
            ring.reserve(segs);
            for (int i = 0; i < segs; ++i) {
                const float a = (float)i / (float)segs * 6.2831853f;
                const glm::vec3 p = hit + (std::cos(a) * T + std::sin(a) * B) * r;
                ImVec2 s;
                if (projectToViewport(viewProj, p, viewportPos, viewportSize, s))
                    ring.push_back(s);
            }

            if (ring.size() >= 2) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddPolyline(ring.data(), (int)ring.size(),
                                IM_COL32(0, 0, 0, 160), ImDrawFlags_Closed, 4.0f);
                dl->AddPolyline(ring.data(), (int)ring.size(),
                                IM_COL32(255, 255, 255, 230), ImDrawFlags_Closed, 2.0f);
            }
        }
    }
}

void ViewportOverlays::renderBillboards(SceneView& view, const Ref<Scene>& scene, bool sceneLive) {
    if (sceneLive || !scene || !view.getFramebuffer())
        return;

    const glm::vec3 cameraPos = glm::vec3(view.getCamera().getPosition());

    // The view renderer unbinds its target when it finishes, so the icons would
    // otherwise land on the default framebuffer and be painted over by the editor
    // UI. Bind the viewport target for the duration of the overlay draws.
    view.getFramebuffer()->bind();
    renderLightBillboards(view, scene, cameraPos);
    renderCameraBillboards(view, scene, cameraPos);
    view.getFramebuffer()->unbind();
}

int32_t ViewportOverlays::performPick(SceneView& view, const Ref<Scene>& scene) {
    // No target yet (first frame, before the viewport sized it): keep the request
    // pending so it runs once the framebuffer exists.
    if (!m_pickingFramebuffer || !scene)
        return -1;

    m_needsPicking = false;

    m_pickingFramebuffer->bind();
    // The picking target is R32I; -1 means "nothing" so entity id 0 is not
    // ambiguous with cleared background.
    RenderCommand::clearInt(-1);

    const glm::mat4 viewProj = view.getCamera().getViewProjectionMatrix();
    Renderer2D::beginPickingScene(viewProj, m_pickingShader);
    Renderer3D::beginPickingScene(viewProj);

    scene->onPickingRender();

    {
        auto lightView = scene->getRegistry().view<LightComponent>();
        if (!lightView.empty() && m_pointLightIcon.isValid()) {
            const auto& viewMatrix = view.getCamera().getViewMatrix();
            const glm::vec3 cameraPos = glm::vec3(view.getCamera().getPosition());

            for (auto entity : lightView) {
                auto& transform = scene->getComponent<TransformComponent>(entity);
                glm::vec3 lightPos = glm::vec3(transform.position);

                float dist = glm::length(lightPos - cameraPos);
                float scale = glm::clamp(dist * 0.08f, 0.3f, 3.0f) * m_lightIconSize;

                glm::mat4 billboardMat = makeBillboard(lightPos, viewMatrix);
                Renderer2D::drawPickingQuad(billboardMat, glm::vec2(scale), entity);
            }
        }
    }

    Renderer2D::endPickingScene();
    Renderer3D::endPickingScene();

    const int32_t entityId = m_pickingFramebuffer->readPixel((uint32_t)m_pickPos.x, (uint32_t)m_pickPos.y);

    m_pickingFramebuffer->unbind();
    return entityId;
}

glm::mat4 ViewportOverlays::makeBillboard(const glm::vec3& position, const glm::mat4& viewMatrix) {
    glm::vec3 cameraPos = glm::vec3(glm::inverse(viewMatrix)[3]);

    glm::vec3 forward = glm::normalize(cameraPos - position);

    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
    glm::vec3 up = glm::normalize(glm::cross(forward, right));

    glm::mat4 billboard(1.0f);
    billboard[0] = glm::vec4(right, 0.0f);
    billboard[1] = glm::vec4(up, 0.0f);
    billboard[2] = glm::vec4(forward, 0.0f);
    billboard[3] = glm::vec4(position, 1.0f);

    return billboard;
}

void ViewportOverlays::renderLightBillboards(SceneView& view, const Ref<Scene>& scene, const glm::vec3& cameraPos) {
    auto lightView = scene->getRegistry().view<LightComponent>();
    if (lightView.empty() || !m_pointLightIcon.isValid()) return;

    glm::mat4 vp = view.getCamera().getProjectionMatrix() * view.getCamera().getViewMatrix();
    const glm::mat4& viewMatrix = view.getCamera().getViewMatrix();

    Renderer2D::setBlending(true);
    RenderCommand::setDepthTesting(false);
    Renderer2D::beginScene(vp);

    for (auto entity : lightView) {
        auto& transform = scene->getComponent<TransformComponent>(entity);
        glm::vec3 lightPos = glm::vec3(transform.position);

        float dist = glm::length(lightPos - cameraPos);
        float scale = glm::clamp(dist * 0.2f, 0.3f, 3.0f) * m_lightIconSize;

        glm::mat4 billboardMat = makeBillboard(lightPos, viewMatrix);
        Renderer2D::drawQuad(billboardMat, glm::vec2(scale), m_pointLightIcon);
    }

    Renderer2D::endScene();
    RenderCommand::setDepthTesting(true);
    Renderer2D::setBlending(false);
}

void ViewportOverlays::renderCameraBillboards(SceneView& view, const Ref<Scene>& scene, const glm::vec3& cameraPos) {
    auto cameraView = scene->getRegistry().view<CameraComponent>();
    if (cameraView.empty() || !m_cameraIcon.isValid()) return;

    glm::mat4 vp = view.getCamera().getProjectionMatrix() * view.getCamera().getViewMatrix();
    const glm::mat4& viewMatrix = view.getCamera().getViewMatrix();

    Renderer2D::setBlending(true);
    RenderCommand::setDepthTesting(false);
    Renderer2D::beginScene(vp);

    // The piloted camera renders the viewport, so its icon would land at the eye
    // -- skip it, same as the frustum overlay.
    const bool sceneDriven = view.isSceneDriven();
    uint32_t activeCam = view.getSceneCameraEntity().getId();
    if (activeCam == 0xFFFFFFFFu) activeCam = scene->getPrimaryCameraEntity().getId();

    for (auto entity : cameraView) {
        if (sceneDriven && entity == activeCam) continue;
        if (!scene->getRegistry().has<TransformComponent>(entity)) continue;
        auto& transform = scene->getComponent<TransformComponent>(entity);
        glm::vec3 cameraOrigin = glm::vec3(transform.position);

        float dist = glm::length(cameraOrigin - cameraPos);
        float scale = glm::clamp(dist * 0.2f, 0.3f, 3.0f) * m_cameraIconSize;

        glm::mat4 billboardMat = makeBillboard(cameraOrigin, viewMatrix);
        Renderer2D::drawQuad(billboardMat, glm::vec2(scale), m_cameraIcon);
    }

    Renderer2D::endScene();
    RenderCommand::setDepthTesting(true);
    Renderer2D::setBlending(false);
}

}

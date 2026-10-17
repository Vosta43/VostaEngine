#include "Viewport/ViewportToolbar.h"

#include "PlaySession.h"

#include "imgui.h"

namespace ve {

namespace {

// Square PNG-icon toggle button for the toolbar. Active state inverts
// Unity-style: light background with a black-tinted icon; inactive is the dark
// button with the icon at full colour.
bool iconToggleButton(const char* id, bool& value, const char* tooltip, const Ref<Texture2D>& icon) {
    const float h = ImGui::GetFrameHeight();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton(id, ImVec2(h, h)))
        value = !value;
    const bool hovered = ImGui::IsItemHovered();
    ImU32 bg = value ? IM_COL32(215, 215, 215, 255)
             : hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                       : ImGui::GetColorU32(ImGuiCol_Button);
    ImU32 tint = value ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + h, p0.y + h), bg, 3.0f);
    if (icon && icon->getRendererID()) {
        // Icon covers the whole button face; the tint alone carries the pressed
        // state, and the background shows through any alpha.
        dl->AddImageRounded((ImTextureID)(uintptr_t)icon->getRendererID(),
                            p0, ImVec2(p0.x + h, p0.y + h),
                            ImVec2(0, 1), ImVec2(1, 0), tint, 3.0f);
    }
    if (hovered)
        ImGui::SetTooltip("%s", tooltip);
    return value;
}

// Square PNG-icon push button (no toggle state). Returns true on click.
bool iconButton(const char* id, const char* tooltip, const Ref<Texture2D>& icon) {
    const float h = ImGui::GetFrameHeight();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(h, h));
    const bool hovered = ImGui::IsItemHovered();
    ImU32 bg = hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                       : ImGui::GetColorU32(ImGuiCol_Button);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + h, p0.y + h), bg, 3.0f);
    if (icon && icon->getRendererID()) {
        dl->AddImageRounded((ImTextureID)(uintptr_t)icon->getRendererID(),
                            p0, ImVec2(p0.x + h, p0.y + h),
                            ImVec2(0, 1), ImVec2(1, 0), IM_COL32(255, 255, 255, 255), 3.0f);
    }
    if (hovered)
        ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

}

void ViewportToolbar::attach() {
    m_wireframeIcon  = Texture2D::create("VostaEditor/resources/icons/cube_tri.png");
    m_groundGridIcon = Texture2D::create("VostaEditor/resources/icons/ground_grid.png");
    m_playIcon       = Texture2D::create("VostaEditor/resources/icons/run.png");
    m_pauseIcon      = Texture2D::create("VostaEditor/resources/icons/pause.png");
    m_stepIcon       = Texture2D::create("VostaEditor/resources/icons/step.png");
    m_stopIcon       = Texture2D::create("VostaEditor/resources/icons/stop.png");
}

void ViewportToolbar::onGuiRender(SceneView& view, const Ref<Scene>& scene, PlaySession& play) {
    // The toolbar is drawn 1.2x above the base UI scale.
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.2f);
    ImGui::PushItemWidth(240.0f);

    // FPS overlay, frame-averaged over a 0.5s window (refresh each window).
    m_fpsTimer += DeltaTime::get().getDeltaTime();
    m_fpsFrames++;
    if (m_fpsTimer >= 0.5f) {
        m_fpsValue = (float)m_fpsFrames / m_fpsTimer;
        m_fpsTimer = 0.0f;
        m_fpsFrames = 0;
    }
    ImGui::Text("FPS: %.0f", m_fpsValue);
    ImGui::SameLine();

    // Editor-camera popup: the free camera's world position, fly speed and view
    // distance. Speed is read back from the controller -- the same value the
    // camera sidecar persists -- instead of being mirrored in a local copy that
    // then overwrites a restored value every frame.
    if (ImGui::Button("Camera"))
        ImGui::OpenPopup("##editorCamera");
    if (ImGui::BeginPopup("##editorCamera")) {
        ImGui::PushItemWidth(220.0f);

        auto& cam = view.getEditorCamera();
        // ImGui drags are float32; at planet scale only the drag DELTA is precise,
        // so edit a float mirror and fold the delta into the authoritative
        // position rather than overwriting it.
        const glm::vec3 posMirror = glm::vec3(cam.getPosition());
        glm::vec3 edited = posMirror;
        if (ImGui::DragFloat3("Position", &edited.x, 0.5f, 0.0f, 0.0f, "%.2f"))
            cam.setPosition(cam.getPosition() + ve::Vec3(edited - posMirror));

        auto& controller = view.getCameraController();
        float moveSpeed = controller.getMoveSpeed();
        if (ImGui::SliderFloat("Speed", &moveSpeed, 0.1f, 256.0f))
            controller.setMoveSpeed(moveSpeed);

        // Boost toggle: multiplies fly speed while ON (applied by EditorLayer). A
        // toggle, not a hold -- the popup closes on click, so a hold can't work.
        const bool boosted = m_cameraBoost;      // pop must match this frame's push
        if (boosted)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(boosted ? "Boost x100: ON" : "Boost x100: OFF"))
            m_cameraBoost = !boosted;
        if (boosted)
            ImGui::PopStyleColor();

        if (!m_farPlaneSeeded) {
            m_farPlane = cam.getFarPlane();
            m_farPlaneSeeded = true;
        }
        // Logarithmic: the range now has to reach a planet radius (~6.4e6), and a
        // linear slider would make the interesting near end unusable.
        if (ImGui::SliderFloat("View Distance", &m_farPlane, 0.1f, 1.0e8f, "%.0f", ImGuiSliderFlags_Logarithmic))
            cam.setFarPlane(m_farPlane);

        // Jump the editor camera above the planet so the whole globe fits: at chart
        // height R the render origin lands R above the surface, i.e. 2R from the
        // centre, and looking straight down frames it. Fly speed tops out far below
        // this altitude, so the button is the only practical way there.
        if (ImGui::Button("Frame planet")) {
            auto atmView = scene->getRegistry().group<TransformComponent, AtmosphereComponent>();
            if (!atmView.empty()) {
                const auto& at = scene->getComponent<AtmosphereComponent>(*atmView.begin());
                const float R = at.atmosphere.planetRadius;
                if (R > 0.0f) {
                    cam.setPosition(ve::Vec3(glm::vec3(0.0f, R, 0.0f)));
                    cam.setYawPitch(0.0f, -89.0f);
                    m_farPlane = R * 4.0f;
                    cam.setFarPlane(m_farPlane);
                }
            }
        }

        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 12.0f);
    iconToggleButton("##wireframe", m_wireframe, "Wireframe", m_wireframeIcon);
    view.setWireframe(m_wireframe);

    ImGui::SameLine(0.0f, 12.0f);
    iconToggleButton("##groundGrid", m_groundGrid, "Ground Grid", m_groundGridIcon);
    view.setGroundGrid(m_groundGrid);

    ImGui::SameLine(0.0f, 12.0f);
    ImGui::Checkbox("TAA", &m_taa);
    view.setTAAEnabled(m_taa);

    // Prototype planet body (fixed-LOD cube-sphere).
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::Checkbox("Globe", &m_showGlobe);
    view.setShowGlobe(m_showGlobe);
    if (m_showGlobe) {
        // The LOD readout: from orbit expect a handful of tiles at Lmax 0; near the
        // ground, many tiles and a deep tree. +k/frame shows the per-frame build
        // budget holding a big jump in required detail to a few tiles a frame.
        const GlobeStats gs = view.getGlobeStats();
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::TextDisabled("Globe: %d tiles  Lmax %d  +%d/frame", gs.tiles, gs.maxLevel, gs.built);
    }

    // Runtime virtual texture inspector.
    ImGui::SameLine(0.0f, 12.0f);
    {
        // Entry order is the RuntimeVirtualTexture::DebugView enum; "Mips" shades each
        // page by its finest resident level, so the streaming pyramid shows directly.
        static const char* kRVTViews[] = { "RVT: Off", "RVT: Albedo", "RVT: Pages", "RVT: Mips" };
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::Combo("##rvtDebug", &m_rvtDebug, kRVTViews, 4))
            view.setRVTDebugView(m_rvtDebug);
    }

    // Play-in-Editor transport. Play starts a session; while live the cluster
    // becomes Pause/Resume, Step and Stop.
    ImGui::SameLine(0.0f, 24.0f);
    if (!play.isLive()) {
        if (iconButton("##play", "Play", m_playIcon))
            if (m_callbacks.onPlay) m_callbacks.onPlay();
    } else {
        if (play.state() == PlaySession::State::Play) {
            if (iconButton("##pause", "Pause", m_pauseIcon))
                play.pause();
        } else {
            if (iconButton("##resume", "Resume", m_playIcon))
                play.resume();
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (iconButton("##step", "Step", m_stepIcon))
            play.stepOnce();
        ImGui::SameLine(0.0f, 6.0f);
        if (iconButton("##stop", "Stop", m_stopIcon))
            if (m_callbacks.onStop) m_callbacks.onStop();
    }

    // View camera picker: the free (editor) camera, or any scene CameraComponent
    // to pilot through the viewport.
    ImGui::SameLine(0.0f, 12.0f);
    if (scene) {
        const bool sceneDriven = view.getCameraSource() == SceneView::CameraSource::Scene;
        const uint32_t activeId = sceneDriven
            ? view.getSceneCameraEntity().getId() : 0xFFFFFFFFu;

        auto entityName = [&](uint32_t id) -> std::string {
            if (scene->getRegistry().has<NameComponent>(id))
                return scene->getComponent<NameComponent>(id).name;
            return "Camera " + std::to_string(id);
        };

        std::string label = sceneDriven ? "Camera" : "Free Camera";
        if (sceneDriven) {
            uint32_t shown = activeId;
            if (shown == 0xFFFFFFFFu) shown = scene->getPrimaryCameraEntity().getId();
            if (shown != 0xFFFFFFFFu) label = entityName(shown);
        }

        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##viewCamera", label.c_str())) {
            if (ImGui::Selectable("Free Camera", !sceneDriven))
                view.setCameraSource(SceneView::CameraSource::Editor);

            for (auto eid : scene->getRegistry().view<CameraComponent>()) {
                const bool sel = sceneDriven && eid == activeId;
                const std::string item = entityName(eid) + "###cam" + std::to_string(eid);
                if (ImGui::Selectable(item.c_str(), sel)) {
                    view.setSceneCameraEntity(scene->getEntity(eid));
                    view.setCameraSource(SceneView::CameraSource::Scene);
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SameLine(0.0f, 24.0f);
    if (ImGui::Checkbox("VSync", &m_vsync))
        Application::get().getWindow()->setVSync(m_vsync);

    ImGui::PopItemWidth();
    ImGui::PopFont();
}

}

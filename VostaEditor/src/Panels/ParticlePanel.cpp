#include "ParticlePanel.h"

#include "AssetSaveRegistry.h"
#include "Asset/Utils.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Particle/EmitterAssetIO.h"
#include "Particle/EmitterCompiler.h"
#include "Particle/Modules.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleBillboard.h"
#include "Renderer/Projection.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer2D.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>

#include <gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace ed = ax::NodeEditor;

namespace ve {

    namespace {

        constexpr int kPreviewSize = 512;

        const char* stageDisplayName(uint32_t stage) {
            switch (static_cast<Stage>(stage)) {
            case Stage::EmitterSpawn:   return "Emitter Spawn";
            case Stage::EmitterUpdate:  return "Emitter Update";
            case Stage::ParticleSpawn:  return "Particle Spawn";
            case Stage::ParticleUpdate: return "Particle Update";
            }
            return "Unknown";
        }

        bool sameGuid(const Guid16& a, const Guid16& b) {
            return a.a == b.a && a.b == b.b;
        }

        // FNV-1a over everything the file persists: stage layout, module identity
        // and parameter values. Gates the dirty flag and the deferred save.
        uint64_t hashAsset(const EmitterAsset& asset) {
            uint64_t h = 1469598103934665603ull;
            auto mix = [&h](const void* data, size_t size) {
                const uint8_t* p = static_cast<const uint8_t*>(data);
                for (size_t i = 0; i < size; ++i) {
                    h ^= p[i];
                    h *= 1099511628211ull;
                }
            };
            auto mixU64 = [&](uint64_t value) { mix(&value, sizeof(value)); };

            for (uint32_t s = 0; s < kStageCount; ++s) {
                mixU64(s);
                for (const ModuleEntry& entry : asset.stages[s]) {
                    mixU64(entry.guid.a);
                    mixU64(entry.guid.b);
                    if (entry.module) {
                        mix(entry.module->name.data(), entry.module->name.size());
                        mixU64(entry.module->version);
                    }
                    else {
                        mixU64(0xFFFFFFFFull);
                    }
                    for (float value : entry.params)
                        mix(&value, sizeof(value));
                }
            }
            return h;
        }

        // Grows/repairs the flat parameter block to the module's declared width,
        // keeping existing values and filling new slots with declared defaults.
        void ensureParams(ModuleEntry& entry) {
            if (!entry.module)
                return;

            uint16_t width = 0;
            for (const ModuleParamDecl& decl : entry.module->params)
                width = static_cast<uint16_t>(width + decl.comps);
            if (entry.params.size() == width)
                return;

            std::vector<float> next(width, 0.0f);
            uint16_t offset = 0;
            for (const ModuleParamDecl& decl : entry.module->params) {
                for (uint8_t c = 0; c < decl.comps; ++c) {
                    const size_t index = static_cast<size_t>(offset) + c;
                    next[index] = index < entry.params.size() ? entry.params[index] : decl.value[c];
                }
                offset = static_cast<uint16_t>(offset + decl.comps);
            }
            entry.params = std::move(next);
        }

    }

    ParticlePanel::~ParticlePanel() {
        if (m_nodeCanvas) {
            ed::DestroyEditor(m_nodeCanvas);
            m_nodeCanvas = nullptr;
        }
    }

    void ParticlePanel::openFile(const std::string& path) {
        m_filePath = path;
        loadFromDisk();
        m_savedHash = m_assetHash;
        *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
        m_externalChange = false;
        AssetSaveRegistry::get().markClean(m_filePath);
    }

    void ParticlePanel::onGuiRender(bool* openFlag) {
        // An outside write bumps the file's write time. Reload in place when we hold
        // no edits; otherwise flag a conflict for the user to resolve.
        if (!m_filePath.empty()) {
            const int64_t liveTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
            if (liveTicks != 0 && liveTicks != *m_diskTicks) {
                *m_diskTicks = liveTicks;
                if (!AssetSaveRegistry::get().isDirty(m_filePath)) {
                    loadFromDisk();
                    m_savedHash = m_assetHash;
                    m_externalChange = false;
                }
                else {
                    m_externalChange = true;
                }
            }
        }

        const std::string title = m_filePath.empty()
            ? std::string("Particle Editor###particleEditor")
            : "Particle Editor - " + std::filesystem::path(m_filePath).stem().string() + "###particleEditor";

        ImGui::SetNextWindowSize(ImVec2(1120.0f, 720.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title.c_str(), openFlag)) {
            ImGui::End();
            return;
        }

        ImGui::BeginChild("##particleViewport", ImVec2((float)kPreviewSize + 24.0f, 0.0f));

        const ImVec2 previewSize((float)kPreviewSize, (float)kPreviewSize);
        const ImVec2 previewPos = ImGui::GetCursorScreenPos();

        if (m_target) {
            ImGui::Image((void*)(intptr_t)m_target->getColorAttachmentRendererID(),
                previewSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        }
        else {
            ImGui::Dummy(previewSize);
        }

        // An Image carries no ID, so ImGui treats a drag over it as a drag on window
        // background and moves the whole window. This invisible button overlays the
        // same rect and captures the drag for the orbit camera instead.
        ImGui::SetCursorScreenPos(previewPos);
        ImGui::InvisibleButton("##particleOrbit", previewSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImGuiIO& io = ImGui::GetIO();
            m_yaw += io.MouseDelta.x * 0.4f;
            m_pitch = glm::clamp(m_pitch + io.MouseDelta.y * 0.4f, -88.0f, 88.0f);
        }
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
            m_distance = glm::max(m_distance * (1.0f - ImGui::GetIO().MouseWheel * 0.1f), 10.0f);

        if (ImGui::Button(m_playing ? "Pause" : "Play", ImVec2(64.0f, 0.0f)))
            m_playing = !m_playing;
        ImGui::SameLine();
        if (ImGui::Button("Restart", ImVec2(72.0f, 0.0f)))
            m_emitter.reset();
        ImGui::SameLine();
        if (ImGui::Button("Frame", ImVec2(64.0f, 0.0f))) {
            m_yaw = -32.0f;
            m_pitch = 14.0f;
            m_distance = 2600.0f;
            m_focus = glm::vec3(0.0f, 0.0f, -700.0f);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("##particleTimeScale", &m_timeScale, 0.01f, 0.0f, 4.0f, "speed %.2fx");

        ImGui::TextDisabled("%u particles | %s", m_emitterReady ? m_emitter.renderDataSet().num : 0u,
            m_playing ? "playing" : "paused");

        if (!m_compileError.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", m_compileError.c_str());
        if (!m_loadWarning.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.62f, 0.25f, 1.0f), "%s", m_loadWarning.c_str());
        if (m_readOnly)
            ImGui::TextColored(ImVec4(1.0f, 0.62f, 0.25f, 1.0f), "Newer format version - read only");

        if (m_externalChange) {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Changed on disk");
            if (ImGui::Button("Reload##particleExternal", ImVec2((float)kPreviewSize * 0.5f, 0.0f))) {
                loadFromDisk();
                AssetSaveRegistry::get().markClean(m_filePath);
                m_savedHash = m_assetHash;
                *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
                m_externalChange = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep mine##particleExternal", ImVec2((float)kPreviewSize * 0.5f, 0.0f))) {
                *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
                m_externalChange = false;
            }
        }

        ImGui::Separator();
        ImGui::BeginDisabled(m_readOnly);
        drawDetails();
        ImGui::EndDisabled();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##particleNodeCanvas");
        drawNodeCanvas();
        ImGui::EndChild();

        ImGui::End();

        // Structural edits first: a successful compile resets the instance, so the
        // tick below starts from a clean buffer.
        compileIfNeeded();

        if (m_emitterReady && !m_compileFailed)
            m_emitter.applyParams(m_asset);

        const uint64_t h = hashAsset(m_asset);
        if (h != m_assetHash) {
            m_assetHash = h;
            markDirty();
        }
        if (!m_filePath.empty() && !m_readOnly && h != m_savedHash && !ImGui::IsAnyItemActive()) {
            saveToFile();
            m_savedHash = h;
            AssetSaveRegistry::get().markClean(m_filePath);
        }

        if (m_playing && m_emitterReady) {
            const float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f) * m_timeScale;
            if (dt > 0.0f)
                m_emitter.tick(dt);
        }

        renderPreview();
    }

    void ParticlePanel::drawNodeCanvas() {
        if (!m_nodeCanvas) {
            ed::Config config;
            config.SettingsFile = nullptr;   // no node layout to persist
            m_nodeCanvas = ed::CreateEditor(&config);
        }
        if (!m_nodeCanvas)
            return;

        ed::SetCurrentEditor(m_nodeCanvas);
        ImGui::PushID("particleEmitterCanvas");
        ed::Begin("ParticleEmitter", ImVec2(0.0f, 0.0f));

        // Place the node once; writing it every frame would pin it under the mouse
        // and stop the user from dragging it.
        if (!m_nodePlaced) {
            ed::SetNodePosition(ed::NodeId(1), ImVec2(60.0f, 60.0f));
            m_nodePlaced = true;
        }

        ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.09f, 0.09f, 0.10f, 1.0f));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(0, 0, 0, 255).Value);
        ed::PushStyleColor(ed::StyleColor_HovNodeBorder, ImColor(180, 180, 60, 255).Value);
        ed::PushStyleColor(ed::StyleColor_SelNodeBorder, ImColor(255, 200, 40, 255).Value);

        ed::BeginNode(ed::NodeId(1));
        ImGui::TextColored(ImVec4(0.96f, 0.78f, 0.45f, 1.0f), "Emitter");
        ImGui::BeginDisabled(m_readOnly);
        drawStageStack();
        ImGui::EndDisabled();
        ed::EndNode();

        ed::PopStyleColor(4);

        ed::Suspend();
        drawAddModulePopup();
        ed::Resume();

        ed::End();
        ImGui::PopID();
        ed::SetCurrentEditor(nullptr);
    }

    void ParticlePanel::drawAddModulePopup() {
        if (m_openAddMenu) {
            ImGui::SetNextWindowPos(ImVec2(m_addMenuX, m_addMenuY), ImGuiCond_Always);
            ImGui::OpenPopup("Add Module##particle");
            m_openAddMenu = false;
            m_addMenuShowing = true;
        }
        if (!m_addMenuShowing)
            return;

        if (!ImGui::BeginPopup("Add Module##particle")) {
            m_addMenuShowing = false;
            return;
        }

        const uint32_t stage = m_addMenuStage >= 0 ? static_cast<uint32_t>(m_addMenuStage) : 0u;
        bool any = false;
        for (const ModuleAsset* module : builtin::builtinModules()) {
            if ((module->validStages & stageBit(static_cast<Stage>(stage))) == 0)
                continue;
            any = true;
            if (ImGui::MenuItem(module->name.c_str())) {
                std::vector<ModuleEntry>& entries = m_asset.stages[stage];
                entries.push_back(createModuleEntry(*module));
                ensureUniqueModuleGuids(m_asset);
                m_hasSelection = true;
                m_selGuid = entries.back().guid;
                ImGui::CloseCurrentPopup();
            }
        }
        if (!any)
            ImGui::TextDisabled("No modules for this stage");

        // The canvas keeps a hovered item id over its whole area, so ImGui's own
        // click-outside close bails out here; close on any click outside a popup.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGuiWindow* hovered = ImGui::GetCurrentContext()->HoveredWindow;
            if (!hovered || (hovered->Flags & ImGuiWindowFlags_Popup) == 0)
                ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void ParticlePanel::drawStageStack() {
        int removeStage = -1;
        size_t removeIndex = 0;
        int moveStage = -1;
        size_t moveIndex = 0;
        int moveDir = 0;

        for (uint32_t s = 0; s < kStageCount; ++s) {
            ImGui::PushID((int)s);
            std::vector<ModuleEntry>& entries = m_asset.stages[s];

            ImGui::Spacing();
            ImGui::TextDisabled("%s (%d)", stageDisplayName(s), (int)entries.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("+")) {
                // Node content is laid out in canvas space; the popup needs the
                // matching screen anchor.
                const ImVec2 anchor = ed::CanvasToScreen(ImGui::GetItemRectMin());
                m_addMenuStage = (int)s;
                m_addMenuX = anchor.x;
                m_addMenuY = anchor.y;
                m_openAddMenu = true;
            }

            for (size_t i = 0; i < entries.size(); ++i) {
                ModuleEntry& entry = entries[i];
                ImGui::PushID((int)i);
                ImGui::Indent(10.0f);

                const bool selected = m_hasSelection && sameGuid(entry.guid, m_selGuid);
                const char* label = entry.module ? entry.module->name.c_str() : "(missing)";
                const float rowWidth = std::max(110.0f, ImGui::CalcTextSize(label).x + 16.0f);
                if (ImGui::Selectable(label, selected, 0, ImVec2(rowWidth, 0.0f))) {
                    m_hasSelection = true;
                    m_selGuid = entry.guid;
                }

                ImGui::SameLine(0.0f, 4.0f);
                ImGui::BeginDisabled(i == 0);
                if (ImGui::SmallButton("Up")) { moveStage = (int)s; moveIndex = i; moveDir = -1; }
                ImGui::EndDisabled();
                ImGui::SameLine(0.0f, 2.0f);
                ImGui::BeginDisabled(i + 1 >= entries.size());
                if (ImGui::SmallButton("Dn")) { moveStage = (int)s; moveIndex = i; moveDir = 1; }
                ImGui::EndDisabled();
                ImGui::SameLine(0.0f, 2.0f);
                if (ImGui::SmallButton("X")) { removeStage = (int)s; removeIndex = i; }

                ImGui::Unindent(10.0f);
                ImGui::PopID();
            }

            ImGui::PopID();
        }

        // Mutate after the widgets are built: erasing or swapping mid-loop would
        // invalidate the indices the buttons just reported.
        if (moveStage >= 0) {
            std::vector<ModuleEntry>& entries = m_asset.stages[moveStage];
            const size_t other = moveDir < 0 ? moveIndex - 1 : moveIndex + 1;
            std::swap(entries[moveIndex], entries[other]);
        }
        if (removeStage >= 0) {
            std::vector<ModuleEntry>& entries = m_asset.stages[removeStage];
            entries.erase(entries.begin() + removeIndex);
            uint32_t stage = 0;
            size_t index = 0;
            if (!findSelected(stage, index))
                m_hasSelection = false;
        }
    }

    void ParticlePanel::drawDetails() {
        uint32_t stage = 0;
        size_t index = 0;
        if (!findSelected(stage, index)) {
            ImGui::TextDisabled("No module selected");
            return;
        }

        ModuleEntry& entry = m_asset.stages[stage][index];
        if (!entry.module) {
            ImGui::TextDisabled("Missing module");
            return;
        }

        ImGui::Text("%s", entry.module->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", stageDisplayName(stage));

        ensureParams(entry);
        if (entry.module->params.empty()) {
            ImGui::TextDisabled("No parameters");
            return;
        }

        for (const ModuleParamDecl& decl : entry.module->params) {
            ImGui::PushID(decl.name.c_str());
            const uint16_t base = paramComponentOffset(*entry.module, decl.name);
            float* value = entry.params.data() + base;
            const float speed = decl.comps == 1 ? 0.5f : 1.0f;
            switch (decl.comps) {
            case 1: ImGui::DragFloat(decl.name.c_str(), value, speed); break;
            case 2: ImGui::DragFloat2(decl.name.c_str(), value, speed); break;
            case 3: ImGui::DragFloat3(decl.name.c_str(), value, speed); break;
            default: ImGui::DragFloat4(decl.name.c_str(), value, speed); break;
            }
            ImGui::PopID();
        }
    }

    bool ParticlePanel::findSelected(uint32_t& stage, size_t& index) const {
        if (!m_hasSelection)
            return false;
        for (uint32_t s = 0; s < kStageCount; ++s) {
            const std::vector<ModuleEntry>& entries = m_asset.stages[s];
            for (size_t i = 0; i < entries.size(); ++i) {
                if (sameGuid(entries[i].guid, m_selGuid)) {
                    stage = s;
                    index = i;
                    return true;
                }
            }
        }
        return false;
    }

    void ParticlePanel::compileIfNeeded() {
        const uint64_t structureHash = computeEmitterStructureHash(m_asset);
        if (m_compiledOnce && structureHash == m_compiledStruct)
            return;

        m_compiledOnce = true;
        m_compiledStruct = structureHash;

        // compileEmitter appends into the layouts it is handed, so it must run on a
        // copy whose layouts start empty; compiling in place would grow the particle
        // stride on every structural edit.
        EmitterAsset candidate;
        for (uint32_t s = 0; s < kStageCount; ++s)
            candidate.stages[s] = m_asset.stages[s];

        if (!compileEmitter(candidate)) {
            m_compileError = "Compile failed (see log)";
            m_compileFailed = true;
            return;
        }

        m_compileError.clear();
        m_compileFailed = false;
        m_asset.compiled = candidate.compiled;
        m_asset.emitterLayout = std::move(candidate.emitterLayout);
        m_asset.particleLayout = std::move(candidate.particleLayout);
        m_asset.structureHash = candidate.structureHash;
        m_emitterReady = m_emitter.initialize(m_asset);
    }

    void ParticlePanel::loadFromDisk() {
        m_asset = EmitterAsset();
        m_compileError.clear();
        m_loadWarning.clear();
        m_readOnly = false;
        m_hasSelection = false;

        std::string error;
        std::string warning;
        bool unsupportedVersion = false;
        if (!loadEmitterAsset(m_filePath, m_asset, &error, &warning, &unsupportedVersion)) {
            m_loadWarning = error.empty() ? "Failed to load particle asset" : error;
            m_readOnly = unsupportedVersion;
            if (!unsupportedVersion)
                m_asset = makeDefaultEmitterAsset();
        }
        else {
            m_loadWarning = warning;
        }

        m_compiledOnce = false;
        m_compileFailed = false;
        compileIfNeeded();
        m_assetHash = hashAsset(m_asset);
    }

    void ParticlePanel::markDirty() {
        if (m_filePath.empty() || m_readOnly)
            return;

        const std::string path = m_filePath;
        const EmitterAsset snapshot = m_asset;
        auto diskTicks = m_diskTicks;
        AssetSaveRegistry::get().markDirty(path, [path, snapshot, diskTicks]() {
            if (saveEmitterAsset(snapshot, path)) {
                *diskTicks = utils::fileWriteTicks(toAbsolute(path));
                ParticleAsset::refreshFromFile(path);
            }
        });
    }

    void ParticlePanel::saveToFile() {
        if (m_filePath.empty() || m_readOnly)
            return;
        if (!saveEmitterAsset(m_asset, m_filePath)) {
            VE_CORE_WARN_PRINT("Particle save failed: %s", m_filePath.c_str());
            return;
        }
        *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
        ParticleAsset::refreshFromFile(m_filePath);
    }

    void ParticlePanel::ensurePreviewResources() {
        if (!m_target)
            m_target = Framebuffer::create((uint32_t)kPreviewSize, (uint32_t)kPreviewSize);

        if (!m_white) {
            const uint8_t white[4] = { 255, 255, 255, 255 };
            m_white = Texture2D::create(1, 1);
            if (m_white)
                m_white->setData((void*)white, 4);
        }

    }

    void ParticlePanel::renderPreview() {
        ensurePreviewResources();
        if (!m_target || !m_white)
            return;

        m_target->bind();
        RenderCommand::setViewport(0, 0, (uint32_t)kPreviewSize, (uint32_t)kPreviewSize);
        RenderCommand::setClearColor(0.055f, 0.06f, 0.075f, 1.0f);
        RenderCommand::clear();

        const float yawRad = glm::radians(m_yaw);
        const float pitchRad = glm::radians(m_pitch);
        const glm::vec3 dir(std::cos(pitchRad) * std::sin(yawRad),
            std::cos(pitchRad) * std::cos(yawRad),
            std::sin(pitchRad));
        const glm::vec3 camPos = m_focus + glm::normalize(dir) * m_distance;
        const glm::mat4 view = glm::lookAt(camPos, m_focus, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::mat4 proj = makePerspective(glm::radians(45.0f), 1.0f, 1.0f, 20000.0f);

        Renderer2D::setBlending(true);
        RenderCommand::setDepthTesting(false);
        Renderer2D::beginScene(proj * view);

        // Ground grid on the XY plane; the builtin modules treat Z as up.
        constexpr float extent = 2000.0f;
        constexpr float step = 200.0f;
        constexpr float thickness = 2.5f;
        for (float x = -extent; x <= extent + 0.5f; x += step) {
            const glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, 0.0f));
            const glm::vec4 tint = std::abs(x) < 0.5f
                ? glm::vec4(0.55f, 0.3f, 0.3f, 0.7f)
                : glm::vec4(0.32f, 0.36f, 0.44f, 0.3f);
            Renderer2D::drawQuad(transform, glm::vec2(thickness, 2.0f * extent), m_white, tint);
        }
        for (float y = -extent; y <= extent + 0.5f; y += step) {
            const glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, y, 0.0f));
            const glm::vec4 tint = std::abs(y) < 0.5f
                ? glm::vec4(0.3f, 0.55f, 0.35f, 0.7f)
                : glm::vec4(0.32f, 0.36f, 0.44f, 0.3f);
            Renderer2D::drawQuad(transform, glm::vec2(2.0f * extent, thickness), m_white, tint);
        }

        Renderer2D::endScene();

        if (m_emitterReady) {
            // The preview draws asset space as-is; the scene view applies the one
            // Z-up -> Y-up map. Particles are their own instanced pass, so they
            // run after the grid's batch has been flushed and bind their program.
            ParticleBillboard::draw(m_asset, m_emitter.renderDataSet(),
                [](const glm::vec3& local) { return local; });
            ParticleBillboard::flush(view, proj);
        }

        RenderCommand::setDepthTesting(true);
        Renderer2D::setBlending(false);

        m_target->unbind();

        // Framebuffer::unbind does not restore the viewport, and the rest of the
        // editor (including ImGui) draws against the window's.
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        RenderCommand::setViewport(0, 0,
            (uint32_t)std::max(display.x, 1.0f), (uint32_t)std::max(display.y, 1.0f));
    }

}

#include "MaterialGraphPanel.h"

#include "AssetSaveRegistry.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/MaterialNodes.h"
#include "Renderer/ThumbnailRenderer.h"
#include "Scene/Archive.h"
#include "Core/AssetConfig.h"

namespace ve {

namespace {

    // FNV-1a over node identity, position, per-type parameters, and links. Changes
    // exactly when the graph's shape or a value does, which is what flags the
    // material dirty (mirrors NoisePanel's hashGraph).
    uint32_t hashMaterialGraph(const MaterialGraph& g) {
        uint32_t h = 2166136261u;
        auto mix = [&](const void* data, size_t n) {
            const uint8_t* p = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
        };
        auto mixf = [&](float v) { mix(&v, sizeof(float)); };

        // Any node kind that carries parameters must be mixed here, or editing it
        // would not flag the material dirty.
        for (const auto& node : g.nodes) {
            mix(&node->id, sizeof(node->id));
            mix(&node->m_pos, sizeof(node->m_pos));
            const NodeType nt = node->m_nodeType;
            mix(&nt, sizeof(nt));

            if (auto* cf = dynamic_cast<ConstantFloatNode*>(node.get()))
                mixf(cf->value);
            else if (auto* c2 = dynamic_cast<Constant2VectorNode*>(node.get())) {
                mixf(c2->value.x); mixf(c2->value.y);
            } else if (auto* c3 = dynamic_cast<Constant3VectorNode*>(node.get())) {
                mixf(c3->value.x); mixf(c3->value.y); mixf(c3->value.z);
            } else if (auto* c4 = dynamic_cast<Constant4VectorNode*>(node.get())) {
                mixf(c4->value.x); mixf(c4->value.y); mixf(c4->value.z); mixf(c4->value.w);
            } else if (auto* tc = dynamic_cast<TextureCoordinateNode*>(node.get())) {
                mixf(tc->uvScale.x); mixf(tc->uvScale.y);
            } else if (auto* ts = dynamic_cast<TextureSamplerNode*>(node.get())) {
                const uint32_t idx = ts->textureHandle.index();
                const uint32_t gen = ts->textureHandle.generation();
                mix(&idx, sizeof(idx)); mix(&gen, sizeof(gen));
            }
        }
        for (const auto& link : g.links) {
            mix(&link.startPin.id, sizeof(uint32_t));
            mix(&link.startPin.pinIndex, sizeof(uint32_t));
            mix(&link.endPin.id, sizeof(uint32_t));
            mix(&link.endPin.pinIndex, sizeof(uint32_t));
        }
        return h;
    }

}

void MaterialGraphPanel::markDirty(const Ref<SingleMaterial>& material, const std::string& assetKey) {
    if (!material || assetKey.empty()) return;

    auto mat = material;
    const std::string path = toAbsolute(assetKey);
    AssetSaveRegistry::get().markDirty(assetKey, [path, mat]() {
        TextArchive ar(path, ArchiveMode::write);
        if (ar.isGood()) mat->serialize(ar);
    });
}

void MaterialGraphPanel::onGuiRender(Ref<SingleMaterial> material, bool& needsRecompile,
                                     bool* openFlag, AssetHandle originalMaterialHandle) {
    if (!material) return;

    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Material Editor", openFlag)) {
        ImGui::End();
        return;
    }

    // The registry key is the authoritative save path: it is re-keyed on
    // rename/move, whereas material->name in the file body can be stale.
    const std::string rel = ResourceManager::getPath<Material>(originalMaterialHandle);
    const std::string assetKey = rel.empty() ? material->name : rel;
    const std::string assetPath = toAbsolute(assetKey);

    // A different material is now bound: adopt its current graph as the baseline so
    // merely opening it is not counted as an unsaved edit.
    if (assetKey != m_lastName) {
        m_lastName = assetKey;
        m_graphHash = hashMaterialGraph(material->graph);
    }

    // --- Toolbar ---
    if (ImGui::Button("Compile")) {

        material->compile();

        if (material->isCompiled) {
            needsRecompile = false;
            VE_CORE_SUCCESS_PRINT("Material compiled successfully");

            // Persist to .veasset file.
            TextArchive ar(assetPath, ArchiveMode::write);
            if (ar.isGood()) {
                material->serialize(ar);
            }

            // Compiling wrote the file, so the graph is clean and the baseline moved.
            AssetSaveRegistry::get().markClean(assetKey);
            m_graphHash = hashMaterialGraph(material->graph);

            if (originalMaterialHandle.isValid()) {
                ThumbnailRenderer::invalidate(originalMaterialHandle);
            }
        }
        else {
            VE_CORE_ERROR_PRINT("Material compilation failed: %s", material->compileError.c_str());
        }
    }
    ImGui::SameLine();
    if (material->isCompiled) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Compiled OK");
    } else if (!material->compileError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", material->compileError.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Not compiled");
    }
    if (AssetSaveRegistry::get().isDirty(assetKey)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "unsaved");
    }

    ImGui::Separator();

    // --- Split layout: left panel (preview + details) | right panel (node editor) ---
    const float leftPanelWidth = 240.0f;
    const float previewHeight   = 200.0f;

    // ===== Left panel =====
    ImGui::BeginChild("##LeftPanel", ImVec2(leftPanelWidth, 0), ImGuiChildFlags_None);

    // Top: material preview sphere
    ImGui::Text("Preview");
    ImGui::Separator();
    if (originalMaterialHandle.isValid()) {
        auto thumbnail = ThumbnailRenderer::getMaterialThumbnail(originalMaterialHandle);
        if (thumbnail && thumbnail->getRendererID() != 0) {
            ImTextureID texId = (ImTextureID)(uintptr_t)thumbnail->getRendererID();
            float availW = ImGui::GetContentRegionAvail().x - 4.0f;
            float size = availW < previewHeight ? availW : previewHeight;
            ImGui::Image(texId, ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));
        }
    } else {
        ImGui::BeginChild("##NoPreview", ImVec2(previewHeight, previewHeight), ImGuiChildFlags_Borders);
        ImGui::EndChild();
    }

    ImGui::Spacing();

    // Bottom: reserved details panel (UE5-style)
    ImGui::Text("Details");
    ImGui::Separator();
    ImGui::BeginChild("##DetailsPanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::TextDisabled("(reserved)");
    ImGui::EndChild();

    ImGui::EndChild(); // ##LeftPanel

    ImGui::SameLine();

    // ===== Right panel: generic node canvas =====
    m_canvas.draw(material->graph, m_schema);

    // The canvas mutates the graph in place (add/delete/link/param edits), so the
    // edit is detected by diffing the hash rather than a returned flag.
    const uint32_t h = hashMaterialGraph(material->graph);
    if (h != m_graphHash) {
        m_graphHash = h;
        markDirty(material, assetKey);
    }

    ImGui::End();
}

} // namespace ve

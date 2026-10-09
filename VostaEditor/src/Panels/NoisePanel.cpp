#include "NoisePanel.h"

#include "AssetSaveRegistry.h"
#include "Noise/NoiseNodes.h"
#include "Noise/NoisePlan.h"
#include "Noise/NoiseGraphResource.h"
#include "Scene/Archive.h"
#include "Core/Log.h"
#include "Core/JobSystem.h"
#include "Core/ResourceManager.h"
#include "Asset/TextureImporter.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

namespace ve {

    namespace {

        // Current on-disk noise format version. Bumped when the layout changes so an
        // older file is discarded rather than misread as a graph. int32_t because a
        // concrete TextArchive hides the base's uint32_t operator<<.
        constexpr int32_t kNoiseFormatVersion = 2;

        // FNV-1a over node ids, each Noise Unit's settings bytes, the Output range,
        // and the links. Changes exactly when the graph's shape or a parameter does,
        // which is what gates the preview bake and the deferred save.
        uint32_t hashGraph(const NoiseGraph& g) {
            uint32_t h = 2166136261u;
            auto mix = [&](const void* data, size_t n) {
                const uint8_t* p = static_cast<const uint8_t*>(data);
                for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
            };

            // Any node kind that carries parameters must be mixed here, or editing
            // it would neither re-bake the preview nor mark the file dirty.
            for (const auto& node : g.nodes) {
                mix(&node->id, sizeof(node->id));
                auto mixf = [&](float v) { mix(&v, sizeof(float)); };

                if (auto* unit = dynamic_cast<NoiseUnitNode*>(node.get()))
                    mix(&unit->settings, sizeof(NoiseSettings));
                else if (auto* c = dynamic_cast<NoiseConstantNode*>(node.get()))
                    mixf(c->value);
                else if (auto* r = dynamic_cast<NoiseRemapNode*>(node.get())) {
                    mixf(r->inMin); mixf(r->inMax); mixf(r->outMin); mixf(r->outMax);
                    mixf(r->clamp ? 1.0f : 0.0f);
                } else if (auto* cl = dynamic_cast<NoiseClampNode*>(node.get())) {
                    mixf(cl->minValue); mixf(cl->maxValue);
                } else if (auto* th = dynamic_cast<NoiseThresholdNode*>(node.get())) {
                    mixf(th->threshold); mixf(th->falloff);
                } else if (auto* e = dynamic_cast<NoiseErfNode*>(node.get())) {
                    mixf(e->divisor);
                } else if (auto* sp = dynamic_cast<NoiseSplineNode*>(node.get())) {
                    for (const auto& p : sp->points) { mixf(p.x); mixf(p.y); }
                } else if (auto* out = dynamic_cast<NoiseOutputNode*>(node.get())) {
                    mixf(out->outputMin); mixf(out->outputMax);
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

    void NoisePanel::ensureDefaultGraph() {
        if (!m_graph.nodes.empty()) return;

        auto unit = CreateRef<NoiseUnitNode>();
        auto out  = CreateRef<NoiseOutputNode>();
        m_graph.addNode(unit, glm::vec2(-280.0f, 0.0f));
        m_graph.addNode(out,  glm::vec2(60.0f, 0.0f));
        m_graph.addLink(unit->m_outputPins[0].id, out->m_inputPins[0].id);
    }

    void NoisePanel::onGuiRender(bool* openFlag) {
        ensureDefaultGraph();
        if (!m_preview)
            rebuildPreview();

        // The "###" leaves the window ID fixed while the title shows the open file.
        const std::string title = m_filePath.empty()
            ? std::string("Noise Editor###noiseEditor")
            : "Noise Editor - " + std::filesystem::path(m_filePath).stem().string() + "###noiseEditor";

        ImGui::SetNextWindowSize(ImVec2(980.0f, 620.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title.c_str(), openFlag)) {
            ImGui::End();
            return;
        }

        // --- Left panel: live preview + export + details ---
        const float previewSize = (float)kPreviewSize;
        ImGui::BeginChild("##noisePreview", ImVec2(previewSize + 64.0f, 0.0f));
        ImGui::Image((void*)(intptr_t)m_preview->getRendererID(), ImVec2(previewSize, previewSize));
        ImGui::TextDisabled("%d x %d - CPU reference", kPreviewSize, kPreviewSize);

        ImGui::BeginDisabled(m_filePath.empty());
        if (ImGui::Button("Export", ImVec2(previewSize, 0.0f)))
            exportTexture();
        ImGui::EndDisabled();
        if (m_filePath.empty())
            ImGui::TextDisabled("not bound to a file");

        // Parameters of the selected node, mirroring the widgets inside it.
        ImGui::Separator();
        if (GraphNode* selected = m_graph.findNode(m_canvas.selectedNodeId()))
            m_schema.drawDetailsContent(selected);
        else
            ImGui::TextDisabled("No node selected");
        ImGui::EndChild();

        ImGui::SameLine();

        // --- Right panel: generic node canvas ---
        m_canvas.draw(m_graph, m_schema);

        // Bake only when the graph actually moved: the preview follows every frame,
        // the file write waits until the widget is released so a drag doesn't stream
        // a hundred saves.
        const uint32_t h = hashGraph(m_graph);
        if (h != m_graphHash) {
            m_graphHash = h;
            rebuildPreview();
            markDirty();
        }
        if (!m_filePath.empty() && h != m_savedHash && !ImGui::IsAnyItemActive()) {
            saveToFile();
            m_savedHash = h;
            AssetSaveRegistry::get().markClean(m_filePath);
        }

        ImGui::End();
    }

    void NoisePanel::exportTexture() {
        const std::filesystem::path src(m_filePath);
        const std::string stem = src.stem().string();

        // Same convention as the file browser's "New ..." items: sit next to the
        // source, and dodge a name clash with a " n" suffix.
        std::error_code ec;
        std::filesystem::path out = src.parent_path() / (stem + "_texture.veasset");
        for (int i = 1; std::filesystem::exists(out, ec); ++i)
            out = src.parent_path() / (stem + "_texture " + std::to_string(i) + ".veasset");

        // The preview buffer is already the baked field; hand it to the texture
        // asset writer so the export is byte-identical to an imported texture.
        TextureResource resource;
        resource.width = kPreviewSize;
        resource.height = kPreviewSize;
        resource.format = TextureFormat::RGBA;
        resource.pixels = m_pixels;

        TextureImporter::serialize(resource, out.string());
        VE_CORE_SUCCESS_PRINT("Exported noise texture: %s", out.string().c_str());
    }

    void NoisePanel::openFile(const std::string& path) {
        m_filePath = path;

        m_graph.nodes.clear();
        m_graph.links.clear();
        m_schema.resetNodeState();

        TextArchive ar(path, ArchiveMode::read);
        if (ar.isGood()) {
            std::string token;
            ar >> token;   // "noise"
            int32_t version = 0;
            ar >> version;
            if (version == kNoiseFormatVersion)
                deserializeNoiseGraph(m_graph, ar);
        }

        ensureDefaultGraph();   // fall back if the file was empty or legacy
        rebuildPreview();
        m_savedHash = m_graphHash;   // don't re-save what was just read
        AssetSaveRegistry::get().markClean(m_filePath);   // freshly read, not unsaved
    }

    void NoisePanel::markDirty() {
        if (m_filePath.empty()) return;

        // Snapshot the graph: the registry outlives this panel, and the save must
        // not depend on the panel still being open when it runs.
        const std::string path = m_filePath;
        const NoiseGraph graph = m_graph;
        AssetSaveRegistry::get().markDirty(path, [path, graph]() {
            TextArchive ar(path, ArchiveMode::write);
            if (!ar.isGood())
                return;
            ar << std::string("noise");
            ar << kNoiseFormatVersion;
            serializeNoiseGraph(graph, ar);
            NoiseGraphResource::refreshFromFile(path);
        });
    }

    void NoisePanel::saveToFile() {
        TextArchive ar(m_filePath, ArchiveMode::write);
        if (!ar.isGood()) {
            VE_CORE_WARN_PRINT("Noise save failed: %s", m_filePath.c_str());
            return;
        }

        ar << std::string("noise");
        ar << kNoiseFormatVersion;
        serializeNoiseGraph(m_graph, ar);

        // Keep the registered resource in step with what was just written, so a
        // terrain can rebuild from edits without an editor restart.
        NoiseGraphResource::refreshFromFile(m_filePath);
    }

    void NoisePanel::rebuildPreview() {
        if (!m_preview)
            m_preview = Texture2D::create(kPreviewSize, kPreviewSize);

        m_pixels.resize((size_t)kPreviewSize * kPreviewSize * 4);

        // The Output node owns the field's display range; without one, fall back to
        // the unit's native [-1, 1].
        auto* out = findNoiseOutput(m_graph);
        const float lo = out ? out->outputMin : -1.0f;
        const float hi = out ? out->outputMax : 1.0f;
        const float range = hi - lo;

        const float half = (float)kPreviewSize * 0.5f;

        // One plan for the whole bake, then rows in parallel: the flattened evaluator
        // touches each node once per sample with no graph walking.
        const NoisePlan plan = buildNoisePlan(m_graph);
        JobSystem::get().parallelFor(0, kPreviewSize, [&](int y) {
            thread_local NoisePlanScratch scratch;
            for (int x = 0; x < kPreviewSize; ++x) {
                const float v = plan.evaluate(scratch, (float)x - half, (float)y - half);
                const float t = range > 1e-6f ? (v - lo) / range : 0.5f;
                const uint8_t g = (uint8_t)std::clamp(t * 255.0f + 0.5f, 0.0f, 255.0f);

                uint8_t* px = &m_pixels[((size_t)y * kPreviewSize + x) * 4];
                px[0] = px[1] = px[2] = g;
                px[3] = 255;
            }
        }).get();

        m_preview->setData(m_pixels.data(), (uint32_t)m_pixels.size());
        m_graphHash = hashGraph(m_graph);
    }

}

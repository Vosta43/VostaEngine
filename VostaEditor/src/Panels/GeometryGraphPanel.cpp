#include "GeometryGraphPanel.h"

#include "AssetSaveRegistry.h"
#include "Asset/AssetLibrary.h"
#include "Asset/BuiltinReousrces.h"
#include "Asset/Utils.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Geometry/GeometryBaker.h"
#include "Geometry/GeometryEvaluator.h"
#include "Geometry/GeometryGraphResource.h"
#include "Geometry/GeometryNodes.h"
#include "Noise/NoiseGraphResource.h"
#include "Noise/NoiseSettings.h"
#include "Sdf/SdfNodes.h"
#include "Renderer/Preprocess/BakeService.h"
#include "Renderer/Projection.h"
#include "Renderer/ThumbnailRenderer.h"
#include "Scene/Archive.h"

#include <imgui.h>

#include <gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace ve {

    namespace {

        // On-disk geometry format version, matching GeometryGraphResource's reader.
        constexpr int32_t kGeometryFormatVersion = 1;

        // FNV-1a over node ids, every node's parameters, and the links. Changes exactly
        // when the graph's shape or a parameter does, which gates the preview and the
        // deferred save. Mirrors NoisePanel::hashGraph.
        uint32_t hashGraph(const GeometryGraph& g) {
            uint32_t h = 2166136261u;
            auto mix = [&](const void* data, size_t n) {
                const uint8_t* p = static_cast<const uint8_t*>(data);
                for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
            };
            auto mixf = [&](float v) { mix(&v, sizeof(float)); };
            auto mixi = [&](int v) { mix(&v, sizeof(int)); };

            for (const auto& node : g.nodes) {
                mix(&node->id, sizeof(node->id));

                if (auto* n = dynamic_cast<IcosphereNode*>(node.get())) {
                    mixf(n->radius); mixi(n->subdivisions);
                    mixf(n->scale.x); mixf(n->scale.y); mixf(n->scale.z);
                } else if (auto* n = dynamic_cast<SubdivideNode*>(node.get())) {
                    mixi(n->iterations); mixi(n->smooth ? 1 : 0);
                } else if (auto* n = dynamic_cast<NoiseDisplaceNode*>(node.get())) {
                    mix(&n->settings, sizeof(NoiseSettings));
                    mixf(n->amplitude); mixf(n->bias); mixi((int)n->mode);
                    mixf(n->direction.x); mixf(n->direction.y); mixf(n->direction.z);
                    mixi(n->sampleInUnitSpace ? 1 : 0);
                } else if (auto* n = dynamic_cast<FieldDisplaceNode*>(node.get())) {
                    mix(n->noisePath.data(), n->noisePath.size());
                    mixf(n->amplitude); mixf(n->bias); mixi((int)n->mode);
                    mixf(n->direction.x); mixf(n->direction.y); mixf(n->direction.z);
                    mixi(n->sampleInUnitSpace ? 1 : 0);
                } else if (auto* n = dynamic_cast<SdfSphereNode*>(node.get())) {
                    mixf(n->center.x); mixf(n->center.y); mixf(n->center.z);
                    mixf(n->radius);
                } else if (auto* n = dynamic_cast<SdfNoiseDisplaceNode*>(node.get())) {
                    mix(n->noisePath.data(), n->noisePath.size());
                    mixf(n->amplitude); mixf(n->bias); mixf(n->frequency);
                } else if (auto* n = dynamic_cast<SdfBooleanNode*>(node.get())) {
                    mixi((int)n->mode); mixf(n->smoothRadius);
                } else if (auto* n = dynamic_cast<MarchingCubesNode*>(node.get())) {
                    mixf(n->boundsMin.x); mixf(n->boundsMin.y); mixf(n->boundsMin.z);
                    mixf(n->boundsMax.x); mixf(n->boundsMax.y); mixf(n->boundsMax.z);
                    mixi(n->resolution);
                } else if (auto* n = dynamic_cast<TransformNode*>(node.get())) {
                    mixf(n->translate.x); mixf(n->translate.y); mixf(n->translate.z);
                    mixf(n->rotateEulerDeg.x); mixf(n->rotateEulerDeg.y); mixf(n->rotateEulerDeg.z);
                    mixf(n->scale.x); mixf(n->scale.y); mixf(n->scale.z);
                } else if (auto* n = dynamic_cast<MeshOutputNode*>(node.get())) {
                    mix(n->name.data(), n->name.size());
                    mixi(n->recalcNormals ? 1 : 0);
                    mixi(n->generateUVs ? 1 : 0);
                    mixi(n->recalcTangents ? 1 : 0);
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

        // Content hashes of the noise graphs the field nodes point at. A field is an
        // external asset, so editing it leaves the geometry graph's own hash untouched;
        // folding these in lets the preview notice and rebuild.
        uint32_t hashFields(const GeometryGraph& g) {
            uint32_t h = 2166136261u;
            for (const auto& node : g.nodes) {
                std::string path;
                if (auto* n = dynamic_cast<FieldDisplaceNode*>(node.get()))
                    path = n->noisePath;
                else if (auto* n = dynamic_cast<SdfNoiseDisplaceNode*>(node.get()))
                    path = n->noisePath;
                else
                    continue;

                uint64_t contentHash = 0;
                const AssetHandle handle = ResourceManager::find<NoiseGraphResource>(path);
                if (handle.isValid())
                    if (Ref<NoiseGraphResource> res = ResourceManager::get<NoiseGraphResource>(handle))
                        contentHash = res->contentHash;

                for (int i = 0; i < 8; ++i) {
                    h ^= static_cast<uint8_t>(contentHash >> (i * 8));
                    h *= 16777619u;
                }
            }
            return h;
        }

    }

    void GeometryGraphPanel::ensureDefaultGraph() {
        if (!m_graph.nodes.empty()) return;

        auto ico    = CreateRef<IcosphereNode>();
        auto noise  = CreateRef<NoiseDisplaceNode>();
        auto output = CreateRef<MeshOutputNode>();
        m_graph.addNode(ico,    glm::vec2(-420.0f, 0.0f));
        m_graph.addNode(noise,  glm::vec2(-140.0f, 0.0f));
        m_graph.addNode(output, glm::vec2(160.0f, 0.0f));
        m_graph.addLink(ico->m_outputPins[0].id,   noise->m_inputPins[0].id);
        m_graph.addLink(noise->m_outputPins[0].id, output->m_inputPins[0].id);
    }

    void GeometryGraphPanel::onGuiRender(bool* openFlag) {
        ensureDefaultGraph();

        if (!m_pipelineReady) {
            m_pipeline.init((uint32_t)kPreviewSize, (uint32_t)kPreviewSize);
            m_previewTarget = Framebuffer::create((uint32_t)kPreviewSize, (uint32_t)kPreviewSize);
            m_pipelineReady = true;
            rebuildPreview();
        }

        // An outside write (an MCP tool, another editor) bumps the file's write time.
        // Reload in place when we hold no edits; otherwise flag a conflict below.
        if (!m_filePath.empty()) {
            const int64_t liveTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
            if (liveTicks != 0 && liveTicks != *m_diskTicks) {
                *m_diskTicks = liveTicks;
                if (!AssetSaveRegistry::get().isDirty(m_filePath)) {
                    loadGraphFromDisk();
                    m_savedHash = m_graphHash;
                    m_externalChange = false;
                } else {
                    m_externalChange = true;
                }
            }
        }

        const std::string title = m_filePath.empty()
            ? std::string("Geometry Editor###geometryEditor")
            : "Geometry Editor - " + std::filesystem::path(m_filePath).stem().string() + "###geometryEditor";

        ImGui::SetNextWindowSize(ImVec2(1040.0f, 660.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title.c_str(), openFlag)) {
            ImGui::End();
            return;
        }

        // --- Left panel: live 3D preview + bake + details ---
        const float previewSize = (float)kPreviewSize;
        ImGui::BeginChild("##geometryPreview", ImVec2(previewSize + 24.0f, 0.0f));

        if (!m_previewError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", m_previewError.c_str());
            ImGui::Dummy(ImVec2(previewSize, previewSize * 0.4f));
        } else {
            ImGui::Image((ImTextureID)(intptr_t)m_previewTarget->getColorAttachmentRendererID(),
                ImVec2(previewSize, previewSize), ImVec2(0, 1), ImVec2(1, 0));

            // Orbit + zoom on the preview image; the FBO is re-rendered below with the
            // updated camera, so the same ImGui::Image shows the new view this frame.
            if (ImGui::IsItemHovered()) {
                ImGuiIO& io = ImGui::GetIO();
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    m_yaw += io.MouseDelta.x * 0.4f;
                    m_pitch = glm::clamp(m_pitch + io.MouseDelta.y * 0.4f, -88.0f, 88.0f);
                    m_previewDirty = true;
                }
                if (io.MouseWheel != 0.0f) {
                    m_distance = glm::max(m_distance * (1.0f - io.MouseWheel * 0.1f), 0.05f);
                    m_previewDirty = true;
                }
            }

            ImGui::TextDisabled("%u verts, %u tris", m_vertexCount, m_triangleCount);
        }

        ImGui::BeginDisabled(m_filePath.empty());
        if (ImGui::Button("Bake to Mesh", ImVec2(previewSize, 0.0f)))
            bakeToMesh();
        ImGui::EndDisabled();
        if (m_filePath.empty())
            ImGui::TextDisabled("not bound to a file");

        if (m_externalChange) {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Changed on disk");
            if (ImGui::Button("Reload##geometryExternal", ImVec2(previewSize * 0.5f, 0.0f))) {
                loadGraphFromDisk();
                AssetSaveRegistry::get().markClean(m_filePath);
                m_savedHash = m_graphHash;
                *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
                m_externalChange = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep mine##geometryExternal", ImVec2(previewSize * 0.5f, 0.0f))) {
                *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
                m_externalChange = false;
            }
        }

        ImGui::Separator();
        if (GraphNode* selected = m_graph.findNode(m_canvas.selectedNodeId()))
            m_schema.drawDetailsContent(selected);
        else
            ImGui::TextDisabled("No node selected");
        ImGui::EndChild();

        ImGui::SameLine();

        // --- Right panel: generic node canvas ---
        m_canvas.draw(m_graph, m_schema);

        // Re-evaluate and re-upload only when the graph moved.
        const uint32_t h = hashGraph(m_graph);
        if (h != m_graphHash) {
            m_graphHash = h;
            markDirty();
        }

        // The preview also follows edits to a referenced noise graph; the geometry
        // file's own hash above deliberately does not, so that edit never rewrites it.
        uint32_t previewHash = h;
        previewHash ^= hashFields(m_graph) + 0x9e3779b9u + (previewHash << 6) + (previewHash >> 2);
        if (previewHash != m_previewHash) {
            m_previewHash = previewHash;
            rebuildPreview();
        }

        if (!m_filePath.empty() && h != m_savedHash && !ImGui::IsAnyItemActive()) {
            saveToFile();
            m_savedHash = h;
            AssetSaveRegistry::get().markClean(m_filePath);
        }

        // On-demand: render on a graph/camera change, and keep rendering until the
        // async atmosphere LUTs have landed so the first frames aren't sky-less.
        // Rendering every frame used to run a whole scene pipeline per frame for a
        // static thumbnail.
        const bool lutReady = BakeService::get().hasAtmosphereLUTs();
        if (m_previewMesh && (m_previewDirty || !lutReady)) {
            renderPreview();
            if (lutReady)
                m_previewDirty = false;   // stay dirty until the sky is in
        }

        ImGui::End();
    }

    void GeometryGraphPanel::rebuildPreview() {
        m_previewDirty = true;

        std::string error;
        Ref<StaticMeshResource> resource = evaluateGeometryGraph(m_graph, &error);
        if (!resource) {
            m_previewError = error.empty() ? "graph did not produce a mesh" : error;
            m_previewMesh = nullptr;
            m_vertexCount = 0;
            m_triangleCount = 0;
            m_graphHash = hashGraph(m_graph);
            return;
        }

        m_previewError.clear();
        m_previewMesh = StaticMesh::create(resource);
        m_vertexCount = m_previewMesh ? m_previewMesh->getVertexCount() : 0;
        m_triangleCount = m_previewMesh ? m_previewMesh->getIndexCount() / 3 : 0;

        // Re-frame the orbit camera on the new bounds so the mesh fills the view.
        if (m_previewMesh) {
            const glm::vec3 mn = m_previewMesh->getAabbMin();
            const glm::vec3 mx = m_previewMesh->getAabbMax();
            m_target = (mn + mx) * 0.5f;
            const float radius = glm::max(glm::length(mx - mn) * 0.5f, 1e-3f);
            m_distance = glm::max(radius * 3.0f, 0.2f);
        }

        m_graphHash = hashGraph(m_graph);
    }

    void GeometryGraphPanel::renderPreview() {
        if (!m_previewTarget)
            return;

        const glm::vec3 mn = m_previewMesh->getAabbMin();
        const glm::vec3 mx = m_previewMesh->getAabbMax();
        const float radius = glm::max(glm::length(mx - mn) * 0.5f, 1e-3f);

        const float yawRad = glm::radians(m_yaw);
        const float pitchRad = glm::radians(m_pitch);
        const glm::vec3 dir(std::cos(pitchRad) * std::sin(yawRad),
                            std::sin(pitchRad),
                            std::cos(pitchRad) * std::cos(yawRad));
        const glm::vec3 camPos = m_target + glm::normalize(dir) * m_distance;

        RenderContext ctx;
        ctx.cameraPosition = camPos;
        ctx.viewMatrix = glm::lookAt(camPos, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
        ctx.projMatrix = makePerspective(glm::radians(45.0f), 1.0f,
            glm::max((m_distance - radius) * 0.5f, 0.01f),
            m_distance + radius * 4.0f + 1.0f);
        ctx.viewPortX = 0;
        ctx.viewPortY = 0;
        ctx.viewPortWidth = kPreviewSize;
        ctx.viewPortHeight = kPreviewSize;

        DrawMeshCommand mesh;
        mesh.transientMesh = m_previewMesh;
        mesh.materialHandle = BuiltinResources::getDefaultMaterial();
        mesh.transform = glm::mat4(1.0f);
        ctx.drawMeshCommands.push_back(mesh);

        DrawLightCommand light;
        light.light = Light();
        light.light.intensity = 7.0f;
        light.light.range = 30.0f;
        light.position = glm::vec3(0.0f, 1.2f, 5.5f);
        ctx.drawLightCommands.push_back(light);

        ThumbnailRenderer::applyPreviewLighting(ctx);

        ctx.outputFrameBuffer = m_previewTarget;
        ctx.presentDiscardBackground = true;

        m_pipeline.render(ctx);
    }

    void GeometryGraphPanel::bakeToMesh() {
        if (m_filePath.empty())
            return;

        const std::filesystem::path src(m_filePath);
        const std::string stem = src.stem().string();

        // First bake picks a free name next to the source; re-bakes overwrite that same
        // file so the asset list doesn't fill with "_mesh 1", "_mesh 2" copies.
        if (m_bakedPath.empty()) {
            std::error_code ec;
            std::filesystem::path out = src.parent_path() / (stem + "_mesh.veasset");
            for (int i = 1; std::filesystem::exists(out, ec); ++i)
                out = src.parent_path() / (stem + "_mesh " + std::to_string(i) + ".veasset");
            m_bakedPath = out.string();
        }

        std::string error;
        if (bakeGeometryGraphToAsset(m_graph, m_bakedPath, &error)) {
            VE_CORE_SUCCESS_PRINT("Baked geometry mesh: %s", m_bakedPath.c_str());
            AssetLibrary::get().notifyChanged();   // let the content browser re-list it
        } else {
            m_previewError = error.empty() ? "bake failed" : error;
            VE_CORE_ERROR_PRINT("Geometry bake failed: %s", m_previewError.c_str());
        }
    }

    void GeometryGraphPanel::loadGraphFromDisk() {
        m_graph.nodes.clear();
        m_graph.links.clear();

        TextArchive ar(m_filePath, ArchiveMode::read);
        if (ar.isGood()) {
            std::string token;
            ar >> token;   // "geometry"
            int32_t version = 0;
            ar >> version;
            if (version == kGeometryFormatVersion)
                deserializeGeometryGraph(m_graph, ar);
        }

        ensureDefaultGraph();   // fall back if the file was empty or legacy
        rebuildPreview();
        m_previewHash = m_graphHash;
        m_previewHash ^= hashFields(m_graph) + 0x9e3779b9u + (m_previewHash << 6) + (m_previewHash >> 2);
    }

    void GeometryGraphPanel::openFile(const std::string& path) {
        m_filePath = path;
        m_bakedPath.clear();
        loadGraphFromDisk();
        m_savedHash = m_graphHash;
        *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
        m_externalChange = false;
        AssetSaveRegistry::get().markClean(m_filePath);
    }

    void GeometryGraphPanel::markDirty() {
        if (m_filePath.empty()) return;

        const std::string path = m_filePath;
        const GeometryGraph graph = m_graph;
        auto diskTicks = m_diskTicks;
        AssetSaveRegistry::get().markDirty(path, [path, graph, diskTicks]() {
            TextArchive ar(path, ArchiveMode::write);
            if (!ar.isGood())
                return;
            ar << std::string("geometry");
            ar << kGeometryFormatVersion;
            serializeGeometryGraph(graph, ar);
            GeometryGraphResource::refreshFromFile(path);
            *diskTicks = utils::fileWriteTicks(toAbsolute(path));
        });
    }

    void GeometryGraphPanel::saveToFile() {
        TextArchive ar(m_filePath, ArchiveMode::write);
        if (!ar.isGood()) {
            VE_CORE_WARN_PRINT("Geometry save failed: %s", m_filePath.c_str());
            return;
        }

        ar << std::string("geometry");
        ar << kGeometryFormatVersion;
        serializeGeometryGraph(m_graph, ar);

        GeometryGraphResource::refreshFromFile(m_filePath);
        *m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
    }

}

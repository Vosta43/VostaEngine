#include "PropertyPanel.h"
#include "AssetBrowserWidget.h"
#include "Scene/Terrain/TerrainMeshBuilder.h"
#include "Scene/Terrain/QuadTreeTerrain.h"
#include "Asset/TextureImporter.h"
#include "Asset/StaticMeshImporter.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>

#define GLM_ENABLE_EXPERIMENTAL
#include <gtc/matrix_transform.hpp>
#include <gtc/matrix_inverse.hpp>
#include <gtc/type_ptr.hpp>
#include <gtx/matrix_decompose.hpp>

namespace ve {

PropertyPanel::PropertyPanel(const Ref<Scene>& sceneContext)
    : m_sceneContext(sceneContext)
{
}

void PropertyPanel::onGuiRender(uint32_t selectedEntity)
{
    ImGui::Begin("Properties");

    if (selectedEntity == UINT32_MAX) {
        ImGui::Text("No entity selected");
        ImGui::End();
        return;
    }

    auto& registry = m_sceneContext->getRegistry();
    auto components = registry.getComponentsForEntity(selectedEntity);

    for (auto& [typeName, compPtr] : components) {
        auto props = ReflectionSystem::getProperties(typeName);
        if (props.empty()) continue;

        // Route known component types to their custom drawers.
        if (typeName == "StaticMeshComponent") {
            drawStaticMeshComponent(static_cast<StaticMeshComponent*>(compPtr));
            continue;
        }

        if (typeName == "TerrainComponent") {
            drawTerrainComponent(selectedEntity, static_cast<TerrainComponent*>(compPtr));
            continue;
        }

        // Default: use the reflection-based generic property drawer.
        if (ImGui::CollapsingHeader(typeName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& prop : props) {
                drawProperty(typeName, compPtr, prop);
            }
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f))) {
        m_showAddComponentPopup = true;
        m_componentSearch[0] = '\0';
    }

    if (m_showAddComponentPopup) {
        ImVec2 popupSize(320.0f, 400.0f);
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(popupSize, ImGuiCond_Appearing);
        ImGui::OpenPopup("Add Component");
        if (ImGui::BeginPopupModal("Add Component", &m_showAddComponentPopup, ImGuiWindowFlags_NoResize)) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##componentSearch", "Search component...", m_componentSearch, sizeof(m_componentSearch));
            ImGui::Separator();

            // Lowercase the query once for case-insensitive matching.
            std::string query = m_componentSearch;
            std::transform(query.begin(), query.end(), query.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            auto& registry = m_sceneContext->getRegistry();
            Entity entity = m_sceneContext->getEntity(selectedEntity);

            ImGui::BeginChild("##componentList");
            std::string currentCategory;
            for (const auto& info : componentRegistry()) {
                // Hide components the entity already has.
                if (info.present(registry, entity)) continue;

                std::string lowerName = info.displayName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lowerName.find(query) == std::string::npos) continue;

                if (info.category != currentCategory) {
                    currentCategory = info.category;
                    ImGui::Separator();
                    ImGui::TextDisabled("%s", currentCategory.c_str());
                }

                if (ImGui::Selectable(info.displayName)) {
                    info.add(registry, entity);
                    m_showAddComponentPopup = false;
                }
            }
            ImGui::EndChild();

            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

// -----------------------------------------------------------------------
// Generic property drawing: routes to per-uiType drawers.
// -----------------------------------------------------------------------
void PropertyPanel::drawProperty(const std::string& typeName, void* compPtr, const ReflectionProperty& prop)
{
    void* ptr = ReflectionSystem::get(typeName, compPtr, prop.name);
    if (!ptr) return;

    // If this property has sub-properties (e.g. a nested struct), recurse.
    auto subProps = ReflectionSystem::getProperties(prop.typeName);
    if (!subProps.empty()) {
        bool open = ImGui::TreeNodeEx(prop.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (open) {
            for (auto& subProp : subProps) {
                drawProperty(prop.typeName, ptr, subProp);
            }
            ImGui::TreePop();
        }
        return;
    }

    // Dispatch by uiType.
    if (prop.uiType == "drag") {
        drawDragProperty(typeName, ptr, prop);
    } else if (prop.uiType == "input") {
        drawInputProperty(ptr, prop);
    } else if (prop.uiType == "material") {
        drawMaterialProperty(ptr, prop);
    } else if (prop.uiType == "texture") {
        drawTextureProperty(ptr);
    }
    else if (prop.uiType == "skyboxtexture") {
        drawSkyboxTextureProperty(ptr);
    } else {
        ImGui::Text("%s", prop.name.c_str());
    }
}

// -----------------------------------------------------------------------
// Per-uiType drawers
// -----------------------------------------------------------------------
void PropertyPanel::drawDragProperty(const std::string& typeName, void* ptr, const ReflectionProperty& prop)
{
    // Per-property "speed=" metadata wins; otherwise scale to the value range
    // so a full drag sweeps ~1% of [min,max] per frame. "format=" overrides the
    // default "%.3f" display when a property needs sub-milli precision.
    float speed = (prop.speed > 0.0f)
        ? prop.speed
        : (prop.maxValue > prop.minValue) ? (prop.maxValue - prop.minValue) * 0.01f : 0.1f;
    const char* fmt = prop.format.empty() ? "%.3f" : prop.format.c_str();

    if (prop.name == "transform") {
        // Special: glm::mat4 -> expose position as XYZ drag.
        glm::mat4& mat = *static_cast<glm::mat4*>(ptr);
        glm::vec3 position = glm::vec3(mat[3]);

        ImGui::Text("Position");
        bool changed = false;

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        changed |= ImGui::DragFloat("X", &position.x, speed, prop.minValue, prop.maxValue);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
        changed |= ImGui::DragFloat("Y", &position.y, speed, prop.minValue, prop.maxValue);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 1.0f, 1.0f));
        changed |= ImGui::DragFloat("Z", &position.z, speed, prop.minValue, prop.maxValue);
        ImGui::PopStyleColor();

        if (changed) {
            mat[3] = glm::vec4(position, 1.0f);
        }
        return;
    }

    if (prop.name == "color") {
        // Special: light color as RGB drag.
        glm::vec3 color = *static_cast<glm::vec3*>(ptr);

        ImGui::Text("Light color");
        bool changed = false;

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        changed |= ImGui::DragFloat("R", &color.x, speed, prop.minValue, prop.maxValue);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
        changed |= ImGui::DragFloat("G", &color.y, speed, prop.minValue, prop.maxValue);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 1.0f, 1.0f));
        changed |= ImGui::DragFloat("B", &color.z, speed, prop.minValue, prop.maxValue);
        ImGui::PopStyleColor();

        if (changed) {
            *static_cast<glm::vec3*>(ptr) = color;
        }
        return;
    }

    // Generic vec2 drag — exposes XY (e.g. cloud wind direction).
    if (prop.typeName == "glm::vec2") {
        glm::vec2& vec = *static_cast<glm::vec2*>(ptr);
        ImGui::DragFloat2(prop.name.c_str(), glm::value_ptr(vec), speed, prop.minValue, prop.maxValue, fmt);
        return;
    }

    // Generic vec3 drag — exposes XYZ.
    if (prop.typeName == "glm::vec3") {
        glm::vec3& vec = *static_cast<glm::vec3*>(ptr);
        ImGui::DragFloat3(prop.name.c_str(), glm::value_ptr(vec), speed, prop.minValue, prop.maxValue, fmt);
        return;
    }

    // Generic vec4 drag — exposes 4 components (e.g. shape FBM weights).
    if (prop.typeName == "glm::vec4") {
        glm::vec4& vec = *static_cast<glm::vec4*>(ptr);
        ImGui::DragFloat4(prop.name.c_str(), glm::value_ptr(vec), speed, prop.minValue, prop.maxValue, fmt);
        return;
    }

    // Integer drag — the generic float path would reinterpret int storage as
    // float, so handle int-typed properties explicitly before that cast.
    if (prop.typeName == "int") {
        int* val = static_cast<int*>(ptr);
        ImGui::DragInt(prop.name.c_str(), val, std::max(1.0f, speed), (int)prop.minValue, (int)prop.maxValue);
        return;
    }

    // Generic float drag — writes directly to ptr in-place.
    float* val = static_cast<float*>(ptr);
    ImGui::DragFloat(prop.name.c_str(), val, speed, prop.minValue, prop.maxValue, fmt);
}

void PropertyPanel::drawInputProperty(void* ptr, const ReflectionProperty& prop)
{
    std::string* val = static_cast<std::string*>(ptr);
    std::vector<char> buf(val->begin(), val->end());
    buf.push_back('\0');
    buf.resize(256, '\0');
    if (ImGui::InputText(prop.name.c_str(), buf.data(), buf.size())) {
        *val = std::string(buf.data());
    }
}

void PropertyPanel::drawMaterialProperty(void* ptr, const ReflectionProperty& prop)
{
    AssetHandle* materialHandle = static_cast<AssetHandle*>(ptr);

    if (!materialHandle->isValid()) {
        ImGui::Text("%s: Invalid", prop.name.c_str());
        return;
    }

    auto material = ResourceManager::get<Material>(*materialHandle);
    if (!material) {
        ImGui::Text("%s: Not loaded", prop.name.c_str());
        return;
    }

    if (ImGui::TreeNode(prop.name.c_str())) {
        drawTexturePropertyWidget("Albedo",   material->albedoMapHandle, *materialHandle);
        drawTexturePropertyWidget("Normal",   material->normalMapHandle, *materialHandle);
        drawTexturePropertyWidget("Metallic", material->metallicMapHandle, *materialHandle);
        drawTexturePropertyWidget("Roughness", material->roughnessMapHandle, *materialHandle);
        drawTexturePropertyWidget("AO",       material->aoMapHandle, *materialHandle);
        ImGui::Separator();
        if (ImGui::Button("Open Material Editor")) {
            if (m_onOpenMaterialEditor && materialHandle->isValid()) {
                m_onOpenMaterialEditor(*materialHandle);
            }
        }
        ImGui::TreePop();
    }
}

void PropertyPanel::drawTextureProperty(void* ptr)
{
    AssetHandle* handle = static_cast<AssetHandle*>(ptr);
    drawTexturePropertyWidget("Texture", *handle, AssetHandle());
}

void PropertyPanel::drawSkyboxTextureProperty(void* ptr)
{
    AssetHandle* handle = static_cast<AssetHandle*>(ptr);

    float imageSize = 64.0f;
    if (handle->isValid()) {
        auto tex = ResourceManager::get<TextureCubeMap>(*handle);
        if (tex && tex->getRendererID() != 0) {
            ImTextureID texID = (ImTextureID)(uintptr_t)tex->getRendererID();
            ImGui::Image(texID, ImVec2(imageSize, imageSize));
        } else {
            ImGui::Button("?", ImVec2(imageSize, imageSize));
        }
    } else {
        ImGui::Button("Empty", ImVec2(imageSize, imageSize));
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Skybox Texture");

    if (AssetBrowser::ResourceCombo<TextureCubeMap>("##combo", *handle,
        [](AssetHandle h, float size) {
            auto tex = ResourceManager::get<TextureCubeMap>(h);
            if (tex && tex->getRendererID() != 0) {
                ImTextureID texID = (ImTextureID)(uintptr_t)tex->getRendererID();
                ImGui::Image(texID, ImVec2(size, size));
            } else {
                ImGui::Button("?", ImVec2(size, size));
            }
        }, 24.0f))
    {
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear")) {
        *handle = AssetHandle();
    }
    
    static AssetHandle m_tempTexture2DHandle;

    if (ImGui::Button("Cast From Texture")) {
        ImGui::OpenPopup("##CastTexturePopup");
    }

    if (ImGui::BeginPopup("##CastTexturePopup")) {
        ImGui::Text("Select Texture2D to convert to CubeMap:");
        ImGui::Separator();

        if (AssetBrowser::ResourceCombo<Texture2D>("##castCombo", m_tempTexture2DHandle,
            [](AssetHandle h, float size) {
                auto tex = ResourceManager::get<Texture2D>(h);
                if (tex && tex->getRendererID() != 0) {
                    ImTextureID texID = (ImTextureID)(uintptr_t)tex->getRendererID();
                    ImGui::Image(texID, ImVec2(size, size));
                }
                else {
                    ImGui::Button("?", ImVec2(size, size));
                }
            }, 24.0f))
        {
            std::string srcPath = ResourceManager::getPath<Texture2D>(m_tempTexture2DHandle);

            AssetHandle existing = ResourceManager::find<TextureCubeMap>(srcPath);
            if (existing.isValid()) {
                *handle = existing;
            }
            else {
                *handle = ResourceManager::store<TextureCubeMap>(srcPath);
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }


    ImGui::EndGroup();
}

// -----------------------------------------------------------------------
// Component-level custom drawers
// -----------------------------------------------------------------------
void PropertyPanel::drawStaticMeshComponent(StaticMeshComponent* comp)
{
    const auto& props = ReflectionSystem::getProperties("StaticMeshComponent");

    if (!ImGui::CollapsingHeader("StaticMeshComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // mesh handle
    for (const auto& prop : props) {
        if (prop.name == "static mesh") {
            drawProperty("StaticMeshComponent", comp, prop);
            break;
        }
    }

    // --- Static mesh thumbnail preview + selector ---
    float thumbSize = 128.0f;
    if (comp->staticMeshHandle.isValid()) {
        auto meshThumb = ThumbnailRenderer::getStaticMeshThumbnail(comp->staticMeshHandle);
        if (meshThumb) {
            ImTextureID texID = (ImTextureID)(uintptr_t)meshThumb->getRendererID();
            ImGui::Image(texID, ImVec2(thumbSize, thumbSize), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            ImGui::Button("?", ImVec2(thumbSize, thumbSize));
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("Empty", ImVec2(thumbSize, thumbSize));
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Static Mesh");

    if (AssetBrowser::ResourceCombo<StaticMesh>("##mesh_combo", comp->staticMeshHandle,
        [](AssetHandle h, float size) {
            auto thumb = ThumbnailRenderer::getStaticMeshThumbnail(h);
            if (thumb && thumb->getRendererID() != 0) {
                ImTextureID texID = (ImTextureID)(uintptr_t)thumb->getRendererID();
                ImGui::Image(texID, ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));
            } else {
                ImGui::Button("?", ImVec2(size, size));
            }
        }, 32.0f))
    {
        // Mesh changed — clear submesh entries so they get re-lazied
        comp->submeshEntries.clear();
        ThumbnailRenderer::invalidateMesh(comp->staticMeshHandle);
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        comp->staticMeshHandle = AssetHandle();
        comp->submeshEntries.clear();
    }
    ImGui::EndGroup();

    // Lazy-populate submesh entries from the loaded mesh.
    if (comp->submeshEntries.empty() && comp->staticMeshHandle.isValid()) {
        auto staticMesh = ResourceManager::get<StaticMesh>(comp->staticMeshHandle);
        if (staticMesh) {
            auto& subMeshes = staticMesh->getSubmeshes();
            for (auto& subMesh : subMeshes) {
                SubmeshEntry entry;
                entry.name = subMesh.name;
                entry.materialHandle = subMesh.materialHandle;
                comp->submeshEntries.push_back(entry);
            }
        }
    }

    // Single material (no submeshes).
    if (comp->submeshEntries.empty()) {
        for (const auto& prop : props) {
            if (prop.name == "material") {
                drawProperty("StaticMeshComponent", comp, prop);
                break;
            }
        }
        return;
    }

    // Multi-submesh UI.
    if (ImGui::TreeNode("Submeshes")) {
        for (size_t i = 0; i < comp->submeshEntries.size(); i++) {
            auto& entry = comp->submeshEntries[i];
            ImGui::PushID(static_cast<int>(i));

            if (ImGui::TreeNode(entry.name.c_str())) {
                auto mat = ResourceManager::get<Material>(entry.materialHandle);
                if (mat) {
                    // Thumbnail preview.
                    auto matTexture = ThumbnailRenderer::getMaterialThumbnail(entry.materialHandle);
                    float imageSize = 128.0f;
                    ImTextureID texID = (ImTextureID)(matTexture->getRendererID());
                    ImGui::Image(texID, ImVec2(imageSize, imageSize));
                    ImGui::SameLine();

                    // Material selector combo.
                    if (AssetBrowser::ResourceCombo<Material>("##combo_material", entry.materialHandle,
                        [](AssetHandle h, float size) {
                            auto thumbnail = ThumbnailRenderer::getMaterialThumbnail(h);
                            if (thumbnail) {
                                ImTextureID thumbID = (ImTextureID)(thumbnail->getRendererID());
                                ImGui::Image(thumbID, ImVec2(size, size));
                            } else {
                                ImGui::Button("?", ImVec2(size, size));
                            }
                        }, 32.0f))
                    {
                    }

                    if (ImGui::Button("Open Editor")) {
                        if (m_onOpenMaterialEditor && entry.materialHandle.isValid()) {
                            m_onOpenMaterialEditor(entry.materialHandle);
                            VE_CORE_INFO_PRINT("Open Material path : %s",ResourceManager::getPath<Material>(entry.materialHandle));
                        }
                        else {
                            VE_CORE_WARN_PRINT("Material is unvalid");
                        }
                    }

                    ImGui::Separator();

                    drawTexturePropertyWidget("Albedo",   mat->albedoMapHandle, entry.materialHandle);
                    drawTexturePropertyWidget("Normal",   mat->normalMapHandle, entry.materialHandle);
                    drawTexturePropertyWidget("Metallic", mat->metallicMapHandle, entry.materialHandle);
                    drawTexturePropertyWidget("Roughness", mat->roughnessMapHandle, entry.materialHandle);
                    drawTexturePropertyWidget("AO",       mat->aoMapHandle, entry.materialHandle);
                } else {
                    ImGui::Text("No material assigned");
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

// -----------------------------------------------------------------------
// Shared: texture selector (preview + search combo + clear)
// -----------------------------------------------------------------------
void PropertyPanel::drawTexturePropertyWidget(const std::string& label, AssetHandle& textureHandle, AssetHandle materialHandle) {
    
    ImGui::PushID(label.c_str());

    // Preview thumbnail.
    float imageSize = 64.0f;
    if (textureHandle.isValid()) {
        auto tex = ResourceManager::get<Texture2D>(textureHandle);
        AssetBrowser::DrawTextureThumbnail(tex, imageSize);
    } 
    else {
        ImGui::Button("Empty", ImVec2(imageSize, imageSize));
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("%s", label.c_str());

    // Searchable resource combo.
    // textureHandle is passed by reference, so ResourceCombo writes directly to it.
    if (AssetBrowser::ResourceCombo<Texture2D>("##combo", textureHandle, [](AssetHandle h, float size) {
            auto tex = ResourceManager::get<Texture2D>(h);
            if (tex && tex->getRendererID() != 0) {
                ImTextureID texID = (ImTextureID)(uintptr_t)tex->getRendererID();
                ImGui::Image(texID, ImVec2(size, size));
            } else {
                ImGui::Button("?", ImVec2(size, size));
            }
        }, 24.0f))
    {
        if (materialHandle.isValid()) {
            ThumbnailRenderer::invalidate(materialHandle);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear")) {
        textureHandle = AssetHandle();
        if (materialHandle.isValid()) {
            ThumbnailRenderer::invalidate(materialHandle);
        }
    }

    ImGui::EndGroup();
    ImGui::PopID();
}

// -----------------------------------------------------------------------
// TerrainComponent custom drawer
// -----------------------------------------------------------------------
void PropertyPanel::drawTerrainComponent(uint32_t entityId, TerrainComponent* comp)
{
    if (!ImGui::CollapsingHeader("TerrainComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // --- Height Map selector ---
    float imageSize = 64.0f;
    if (comp->heightMapHandle.isValid()) {
        auto tex = ResourceManager::get<Texture2D>(comp->heightMapHandle);
        AssetBrowser::DrawTextureThumbnail(tex, imageSize);
    } else {
        ImGui::Button("Empty", ImVec2(imageSize, imageSize));
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Height Map");

    AssetBrowser::ResourceCombo<Texture2D>("##heightmap_combo", comp->heightMapHandle,
        [](AssetHandle h, float size) {
            auto tex = ResourceManager::get<Texture2D>(h);
            if (tex && tex->getRendererID() != 0) {
                ImTextureID texID = (ImTextureID)(uintptr_t)tex->getRendererID();
                ImGui::Image(texID, ImVec2(size, size));
            } else {
                ImGui::Button("?", ImVec2(size, size));
            }
        }, 24.0f);

    ImGui::SameLine();
    if (ImGui::Button("Clear##heightmap")) {
        comp->heightMapHandle = AssetHandle();
        comp->bDirty = true;
    }
    ImGui::EndGroup();

    // --- Material selector ---
    ImGui::Separator();
    ImGui::Text("Material");
    AssetBrowser::ResourceCombo<Material>("##terrain_material_combo", comp->terrainMaterialHandle,
        [](AssetHandle h, float size) {
            auto thumbnail = ThumbnailRenderer::getMaterialThumbnail(h);
            if (thumbnail) {
                ImTextureID thumbID = (ImTextureID)(thumbnail->getRendererID());
                ImGui::Image(thumbID, ImVec2(size, size));
            } else {
                ImGui::Button("?", ImVec2(size, size));
            }
        }, 24.0f);
    ImGui::SameLine();
    if (ImGui::Button("Clear##material")) {
        comp->terrainMaterialHandle = AssetHandle();
    }
    ImGui::Separator();
    if (ImGui::Button("Open Material Editor")) {
        if (m_onOpenMaterialEditor && comp->terrainMaterialHandle.isValid()) {
            m_onOpenMaterialEditor(comp->terrainMaterialHandle);
        }
    }

    // --- Parameters ---
    ImGui::Separator();
    ImGui::DragFloat("Tile Size", &comp->tileSize, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Height Scale", &comp->heightScale, 0.01f, 0.01f, 100.0f);

    // --- Quadtree LOD ---
    ImGui::Separator();
    ImGui::Text("LOD (Quadtree)");
    ImGui::DragInt("Max Depth", &comp->maxDepth, 1.0f, 1, 12);
    ImGui::DragInt("Chunk Segments", &comp->segments, 1.0f, 4, 64);
    ImGui::DragFloat("Detail", &comp->lodDetail, 0.5f, 1.0f, 128.0f);
    ImGui::DragFloat("Render Distance", &comp->renderDistance, 100.0f, 100.0f, 100000.0f);

    // --- Dirty indicator ---
    if (comp->bDirty && comp->heightMapHandle.isValid()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Mesh needs regeneration");
    }

    // --- Generate button ---
    ImGui::Separator();
    bool canGenerate = comp->heightMapHandle.isValid();
    if (!canGenerate) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Edit Terrain", ImVec2(-1, 0))) {
        
    }
    if (ImGui::Button("Generate Terrain", ImVec2(-1, 0))) {
        std::string heightMapPath = toAbsolute(ResourceManager::getPath<Texture2D>(comp->heightMapHandle));
        if (!heightMapPath.empty()) {
            auto textureResource = TextureImporter::importFromFile(heightMapPath);
            if (textureResource) {
                if (!comp->quadtree) comp->quadtree = CreateRef<QuadTreeTerrain>();
                auto mesh = comp->quadtree->build(
                    textureResource, comp->tileSize, comp->heightScale,
                    comp->maxDepth, comp->segments);

                if (mesh) {
                    // Remove old mesh from storage if present
                    if (comp->generatedMeshHandle.isValid()) {
                        auto& storage = ResourceManager::getStorage<StaticMesh>();
                        storage.remove(comp->generatedMeshHandle);
                    }

                    // Register new mesh with a unique synthetic path
                    std::string meshPath = "__terrain/" + std::to_string(entityId);
                    auto& storage = ResourceManager::getStorage<StaticMesh>();
                    comp->generatedMeshHandle = storage.store(meshPath, mesh);
                    
                    comp->bDirty = false;

                    VE_CORE_INFO_PRINT("Terrain mesh generated: %d vertices",
                        mesh->getVertexCount());
                } else {
                    VE_CORE_ERROR_PRINT("Terrain mesh generation failed");
                }
            } else {
                VE_CORE_ERROR_PRINT("Failed to import heightmap: %s", heightMapPath.c_str());
            }
        }
    }
    if (!canGenerate) {
        ImGui::EndDisabled();
    }

    // --- Preview thumbnail of generated mesh ---
    if (comp->generatedMeshHandle.isValid() && !comp->bDirty) {
        ImGui::Separator();
        ImGui::Text("Generated Mesh Preview");

        auto mesh = ResourceManager::get<StaticMesh>(comp->generatedMeshHandle);
        if (mesh) {
            ImGui::Text("Vertices: %u", mesh->getVertexCount());
            ImGui::Text("Indices: %u", mesh->getIndexCount());
        }

        auto thumb = ThumbnailRenderer::getStaticMeshThumbnail(comp->generatedMeshHandle);
        if (thumb) {
            float thumbSize = 128.0f;
            ImTextureID texID = (ImTextureID)(uintptr_t)thumb->getRendererID();
            ImGui::Image(texID, ImVec2(thumbSize, thumbSize), ImVec2(0, 1), ImVec2(1, 0));
        }
    }
}

} // namespace ve

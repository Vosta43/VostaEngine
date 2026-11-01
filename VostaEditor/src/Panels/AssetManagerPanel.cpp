#include "AssetManagerPanel.h"
#include "AssetVisuals.h"

#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/SingleMaterial.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/ThumbnailRenderer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

#include <imgui.h>

namespace ve {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string iconAsset(const char* file) {
    return std::string("VostaEditor/resources/icons/") + file;
}

} // namespace

AssetManagerPanel::AssetManagerPanel() {
    loadIcons();
}

void AssetManagerPanel::loadIcons() {
    auto load = [](const char* file) {
        return std::shared_ptr<Texture2D>(Texture2D::create(iconAsset(file)));
    };

    m_iconNoise = load("noise_resource_2.png");
    m_iconParticle = load("particle.png");
    m_iconTerrainData = load("terrain_data.png");
    m_iconTerrain = load("terrain.png");
    m_iconGeometry = load("pcg_graph.png");
    m_iconHud = load("source_code.png");
    m_iconSound = load("sound.png");
    m_iconMaterial = load("image.png");
    m_iconFallback = load("not_imported_file.png");
}

void AssetManagerPanel::setOnOpenAsset(OpenAssetCallback callback) {
    m_onOpen = std::move(callback);
}

void AssetManagerPanel::requestRefresh() {
    m_refreshRequested = true;
}

void AssetManagerPanel::refreshCatalogue() {
    AssetLibrary::get().refresh();
    m_thumbCache.clear();

    m_categories.clear();
    m_typeDisplayNames.clear();
    for (const AssetTypeInfo& type : assetTypes()) {
        if (std::find(m_categories.begin(), m_categories.end(), type.category) == m_categories.end())
            m_categories.push_back(type.category);
        m_typeDisplayNames.emplace(type.typeKey, type.displayName);
    }

    if (m_categoryIndex > static_cast<int>(m_categories.size()))
        m_categoryIndex = 0;

    rebuildTypeOptions();
    rebuildView();
}

void AssetManagerPanel::rebuildTypeOptions() {
    m_typeOptions.clear();

    const std::string category = m_categoryIndex > 0 ? m_categories[m_categoryIndex - 1] : std::string();
    for (const AssetTypeInfo& type : assetTypes()) {
        if (category.empty() || type.category == category)
            m_typeOptions.emplace_back(type.typeKey, type.displayName);
    }

    if (m_typeIndex > static_cast<int>(m_typeOptions.size()))
        m_typeIndex = 0;
}

void AssetManagerPanel::rebuildView() {
    m_view.clear();

    const std::string query = toLower(m_search);
    const std::string category = m_categoryIndex > 0 ? m_categories[m_categoryIndex - 1] : std::string();
    const std::string typeKey = m_typeIndex > 0 ? m_typeOptions[m_typeIndex - 1].first : std::string();

    for (const AssetEntry& entry : AssetLibrary::get().list()) {
        if (!category.empty() && entry.category != category)
            continue;
        if (!typeKey.empty() && entry.typeKey != typeKey)
            continue;

        if (!query.empty()) {
            auto typeName = m_typeDisplayNames.find(entry.typeKey);
            const std::string display = typeName != m_typeDisplayNames.end() ? typeName->second : entry.typeKey;
            if (toLower(entry.name).find(query) == std::string::npos &&
                toLower(entry.path).find(query) == std::string::npos &&
                toLower(display).find(query) == std::string::npos)
                continue;
        }

        m_view.push_back(&entry);
    }

    const auto primaryKey = [this](const AssetEntry& entry) {
        switch (m_sortKey) {
            case 1: {
                auto it = m_typeDisplayNames.find(entry.typeKey);
                return toLower(it != m_typeDisplayNames.end() ? it->second : entry.typeKey);
            }
            case 2: return toLower(entry.category);
            case 3: return toLower(entry.path);
            default: return toLower(assetvis::displayName(entry.name));
        }
    };

    std::stable_sort(m_view.begin(), m_view.end(), [&](const AssetEntry* a, const AssetEntry* b) {
        const std::string ka = primaryKey(*a);
        const std::string kb = primaryKey(*b);
        if (ka != kb)
            return m_sortAsc ? ka < kb : ka > kb;
        return m_sortAsc ? a->path < b->path : a->path > b->path;
    });
}

std::shared_ptr<Texture2D> AssetManagerPanel::thumbnailFor(const AssetEntry& entry) {
    auto cached = m_thumbCache.find(entry.path);
    if (cached != m_thumbCache.end())
        return cached->second;

    std::shared_ptr<Texture2D> texture;
    const std::string& type = entry.typeKey;

    if (type == "texture") {
        texture = assetvis::texturePreviewForAsset(entry.path);
    }
    else if (type == "material" || type == "layered_material") {
        AssetHandle handle = ResourceManager::find<Material>(entry.path);
        if (!handle.isValid())
            handle = ResourceManager::store<Material>(entry.path);
        if (handle.isValid())
            texture = ThumbnailRenderer::getMaterialThumbnail(handle);
        if (!texture)
            texture = m_iconMaterial;
    }
    else if (type == "staticmesh") {
        AssetHandle handle = ResourceManager::find<StaticMesh>(entry.path);
        if (!handle.isValid())
            handle = ResourceManager::store<StaticMesh>(entry.path);
        if (handle.isValid())
            texture = ThumbnailRenderer::getStaticMeshThumbnail(handle);
        if (!texture)
            texture = m_iconGeometry;
    }
    else if (type == "material_layer") {
        // A layer has no baked sphere thumbnail of its own; its identity is its
        // albedo map.
        AssetHandle handle = ResourceManager::find<MaterialLayerAsset>(entry.path);
        if (!handle.isValid())
            handle = ResourceManager::store<MaterialLayerAsset>(entry.path);
        if (auto layer = ResourceManager::get<MaterialLayerAsset>(handle)) {
            AssetHandle albedo;
            if (auto material = std::dynamic_pointer_cast<SingleMaterial>(
                    ResourceManager::get<Material>(layer->layer.material)))
                albedo = material->albedoMapHandle;
            const std::string albedoPath = ResourceManager::getPath<Texture2D>(albedo);
            if (!albedoPath.empty())
                texture = assetvis::texturePreviewForAsset(albedoPath);
        }
        if (!texture)
            texture = m_iconMaterial;
    }
    else if (type == "noise") {
        texture = m_iconNoise;
    }
    else if (type == "particle") {
        texture = m_iconParticle;
    }
    else if (type == "terrain_data") {
        texture = m_iconTerrainData;
    }
    else if (type == "terrain_rule" || type == "scene") {
        texture = m_iconTerrain;
    }
    else if (type == "geometry") {
        texture = m_iconGeometry;
    }
    else if (type == "hud") {
        texture = m_iconHud;
    }
    else if (type == "sound") {
        texture = m_iconSound;
    }

    if (!texture)
        texture = m_iconFallback;

    m_thumbCache.emplace(entry.path, texture);
    return texture;
}

void AssetManagerPanel::onGuiRender(bool* openFlag) {
    if (m_refreshRequested) {
        m_refreshRequested = false;
        refreshCatalogue();
    }

    ImGui::SetNextWindowSize(ImVec2(920.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Manager", openFlag)) {
        ImGui::End();
        return;
    }

    bool viewDirty = false;

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputTextWithHint("##search", "Search assets...", m_search, sizeof(m_search)))
        viewDirty = true;

    ImGui::SameLine();
    const char* categoryLabel = m_categoryIndex == 0
        ? "All Categories" : m_categories[m_categoryIndex - 1].c_str();
    if (ImGui::BeginCombo("##category", categoryLabel)) {
        for (int i = 0; i <= static_cast<int>(m_categories.size()); ++i) {
            const char* label = i == 0 ? "All Categories" : m_categories[i - 1].c_str();
            if (ImGui::Selectable(label, m_categoryIndex == i)) {
                m_categoryIndex = i;
                m_typeIndex = 0;
                rebuildTypeOptions();
                viewDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    const char* typeLabel = m_typeIndex == 0
        ? "All Types" : m_typeOptions[m_typeIndex - 1].second.c_str();
    if (ImGui::BeginCombo("##type", typeLabel)) {
        for (int i = 0; i <= static_cast<int>(m_typeOptions.size()); ++i) {
            const char* label = i == 0 ? "All Types" : m_typeOptions[i - 1].second.c_str();
            if (ImGui::Selectable(label, m_typeIndex == i)) {
                m_typeIndex = i;
                viewDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    const char* sortLabels[] = { "Name", "Type", "Category", "Path" };
    if (ImGui::BeginCombo("##sort", sortLabels[m_sortKey])) {
        for (int i = 0; i < 4; ++i) {
            if (ImGui::Selectable(sortLabels[i], m_sortKey == i)) {
                m_sortKey = i;
                viewDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button(m_sortAsc ? "Asc" : "Desc")) {
        m_sortAsc = !m_sortAsc;
        viewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        refreshCatalogue();

    if (viewDirty)
        rebuildView();

    const std::string countText = std::to_string(m_view.size()) + " / " +
        std::to_string(AssetLibrary::get().list().size()) + " assets";
    const float countWidth = ImGui::CalcTextSize(countText.c_str()).x;
    ImGui::SameLine();
    const float countX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - countWidth;
    if (countX > ImGui::GetCursorPosX())
        ImGui::SameLine(countX);
    ImGui::TextDisabled("%s", countText.c_str());

    ImGui::Separator();

    const float lineH = ImGui::GetTextLineHeight();
    const float spacingY = ImGui::GetStyle().ItemSpacing.y;
    const float textBlockH = lineH * 2.0f + spacingY;
    const float cellW = assetvis::kTile + assetvis::kPad * 2.0f;
    const float sepYOffset = assetvis::kPad + assetvis::kTile + assetvis::kPad;
    const float cellH = sepYOffset + assetvis::kPad + textBlockH + assetvis::kPad;

    const float availW = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(availW / cellW);
    if (columns < 1) columns = 1;
    const float colW = availW / static_cast<float>(columns);
    const ImVec2 gridMin = ImGui::GetCursorScreenPos();

    const int total = static_cast<int>(m_view.size());
    if (total == 0) {
        ImGui::TextDisabled("No assets match the current filters.");
        ImGui::End();
        return;
    }

    const int rows = (total + columns - 1) / columns;

    // Only the rows inside the window's clip rect are drawn, so thumbnails for
    // off-screen material/mesh assets are never baked.
    const ImVec2 clipMin = ImGui::GetWindowDrawList()->GetClipRectMin();
    const ImVec2 clipMax = ImGui::GetWindowDrawList()->GetClipRectMax();
    int firstRow = static_cast<int>(std::floor((clipMin.y - gridMin.y) / cellH));
    int lastRow = static_cast<int>(std::ceil((clipMax.y - gridMin.y) / cellH));
    firstRow = std::max(firstRow, 0);
    lastRow = std::min(lastRow, rows);
    if (lastRow < firstRow)
        lastRow = firstRow;

    // t > 0 fades a colour toward white, t < 0 toward black.
    const auto shade = [](ImU32 col, float t) -> ImU32 {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
        const float target = t < 0.0f ? 0.0f : 1.0f;
        const float k = t < 0.0f ? -t : t;
        c.x += (target - c.x) * k;
        c.y += (target - c.y) * k;
        c.z += (target - c.z) * k;
        return ImGui::GetColorU32(c);
    };

    std::string openPath;

    for (int row = firstRow; row < lastRow; ++row) {
        for (int col = 0; col < columns; ++col) {
            const int idx = row * columns + col;
            if (idx >= total)
                break;

            const AssetEntry& entry = *m_view[idx];
            ImGui::PushID(entry.path.c_str());

            const ImVec2 cellMin(gridMin.x + col * colW, gridMin.y + row * cellH);
            const ImVec2 cellMax(cellMin.x + colW, cellMin.y + cellH);

            ImGui::SetCursorScreenPos(cellMin);
            ImGui::InvisibleButton("##cell", ImVec2(colW, cellH));
            const bool hovered = ImGui::IsItemHovered();
            const bool selected = (m_selectedPath == entry.path);

            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_selectedPath = entry.path;
                openPath = entry.path;
            }
            else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_selectedPath = entry.path;
            }

            if (hovered) {
                auto typeName = m_typeDisplayNames.find(entry.typeKey);
                ImGui::SetTooltip("%s\n%s / %s", entry.path.c_str(),
                                  typeName != m_typeDisplayNames.end() ? typeName->second.c_str() : entry.typeKey.c_str(),
                                  entry.category.c_str());
            }

            ImDrawList* dl = ImGui::GetWindowDrawList();

            const ImU32 base = selected ? IM_COL32(58, 66, 84, 255)
                             : hovered  ? IM_COL32(68, 68, 68, 255)
                                        : IM_COL32(48, 48, 48, 255);
            dl->AddRectFilled(cellMin, cellMax, base, assetvis::kRounding);

            const ImU32 accent = assetvis::accentColorForTypeKey(entry.typeKey);

            const ImU32 stripTop = shade(base, 0.30f);
            const ImU32 stripBot = shade(base, -0.10f);
            if (cellMax.y - assetvis::kRounding > sepYOffset + cellMin.y) {
                dl->AddRectFilledMultiColor(
                    ImVec2(cellMin.x, cellMin.y + sepYOffset),
                    ImVec2(cellMax.x, cellMax.y - assetvis::kRounding),
                    stripTop, stripTop, stripBot, stripBot);
                dl->AddRectFilled(ImVec2(cellMin.x, cellMax.y - assetvis::kRounding), cellMax,
                                  stripBot, assetvis::kRounding, ImDrawFlags_RoundCornersBottom);
            }

            const ImVec2 imgAreaMin(cellMin.x + (colW - assetvis::kTile) * 0.5f,
                                    cellMin.y + assetvis::kPad);
            const ImVec2 imgAreaMax(imgAreaMin.x + assetvis::kTile, imgAreaMin.y + assetvis::kTile);

            float thumbZoom = 1.0f;
            if (entry.typeKey == "material" || entry.typeKey == "layered_material")
                thumbZoom = 1.3f;

            auto icon = thumbnailFor(entry);
            if (icon && icon->getRendererID()) {
                const float tw = static_cast<float>(icon->getWidth());
                const float th = static_cast<float>(icon->getHeight());
                float scale = 1.0f;
                if (tw > 0.0f && th > 0.0f) {
                    const float sx = assetvis::kTile / tw, sy = assetvis::kTile / th;
                    scale = (sx < sy ? sx : sy) * thumbZoom;
                }
                const ImVec2 imgSize(tw * scale, th * scale);
                const ImVec2 imgMin(imgAreaMin.x + (assetvis::kTile - imgSize.x) * 0.5f,
                                    imgAreaMin.y + (assetvis::kTile - imgSize.y) * 0.5f);
                dl->AddImage((ImTextureID)(uintptr_t)icon->getRendererID(),
                             imgMin, ImVec2(imgMin.x + imgSize.x, imgMin.y + imgSize.y),
                             ImVec2(0, 1), ImVec2(1, 0));
            }
            else {
                dl->AddRectFilled(imgAreaMin, imgAreaMax, IM_COL32(60, 60, 60, 255), 4.0f);
            }

            const float sepY = cellMin.y + sepYOffset;
            dl->AddLine(ImVec2(cellMin.x, sepY), ImVec2(cellMax.x, sepY), accent, 3.0f);

            std::string line1, line2;
            assetvis::wrapTwoLines(assetvis::displayName(entry.name), colW - assetvis::kPad * 2.0f,
                                   line1, line2);

            const ImU32 textCol = IM_COL32(225, 225, 225, 255);
            const float textTop = sepY + assetvis::kPad;
            const float w1 = ImGui::CalcTextSize(line1.c_str()).x;
            dl->AddText(ImVec2(cellMin.x + (colW - w1) * 0.5f, textTop), textCol, line1.c_str());
            if (!line2.empty()) {
                const float w2 = ImGui::CalcTextSize(line2.c_str()).x;
                dl->AddText(ImVec2(cellMin.x + (colW - w2) * 0.5f, textTop + lineH + spacingY),
                            textCol, line2.c_str());
            }

            if (selected)
                dl->AddRect(cellMin, cellMax, ImGui::GetColorU32(ImGuiCol_ButtonActive),
                            assetvis::kRounding, 0, 2.0f);

            ImGui::PopID();
        }
    }

    // Reserve the full grid height so the window scrolls to the last row.
    ImGui::SetCursorScreenPos(ImVec2(gridMin.x, gridMin.y + rows * cellH));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    if (!openPath.empty() && m_onOpen)
        m_onOpen(openPath);

    ImGui::End();
}

} // namespace ve

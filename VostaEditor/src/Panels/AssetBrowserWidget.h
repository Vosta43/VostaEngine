#pragma once

#include <string>
#include <vector>
#include <functional>

#include "Core/Core.h"
#include "Core/AssetHandle.h"
#include "Core/ResourceManager.h"

#include "imgui.h"

namespace ve {
namespace AssetBrowser {

// Returns a filtered list of (path, handle) pairs for resource type T.
template<typename T>
std::vector<std::pair<std::string, AssetHandle>> FilterResources(const char* filter) {
    std::vector<std::pair<std::string, AssetHandle>> result;
    auto& entries = ResourceManager::getStorage<T>().entries();
    std::string filterStr = filter ? filter : "";
    for (auto& [path, handle] : entries) {
        if (filterStr.empty() || path.find(filterStr) != std::string::npos) {
            result.emplace_back(path, handle);
        }
    }
    return result;
}

// Renders a search-filterable combo box listing all resources of type T.
// drawItemPreview is an optional callback to render a thumbnail per row.
// Returns true if the user selected a new asset (outHandle is updated).
template<typename T>
bool ResourceCombo(
    const char* label,
    AssetHandle& outHandle,
    std::function<void(AssetHandle, float)> drawItemPreview = nullptr,
    float itemHeight = 24.0f)
{
    bool changed = false;
    ImGui::PushID(label);

    std::string currentName = "None";
    if (outHandle.isValid()) {
        currentName = ResourceManager::getPathByHandle<T>(outHandle);
    }

    if (ImGui::BeginCombo("##combo", currentName.c_str(), ImGuiComboFlags_HeightLarge)) {
        static char searchBuffer[128] = "";
        ImGui::InputText("Filter", searchBuffer, sizeof(searchBuffer));
        ImGui::Separator();

        auto filtered = FilterResources<T>(searchBuffer);

        for (auto& [path, handle] : filtered) {
            ImGui::PushID(static_cast<int>(handle.index()));
            bool isSelected = (handle == outHandle);

            if (ImGui::Selectable("##sel", isSelected, 0, ImVec2(0, itemHeight))) {
                outHandle = handle;
                changed = true;
                ImGui::CloseCurrentPopup();
            }

            if (drawItemPreview) {
                ImGui::SameLine();
                drawItemPreview(handle, itemHeight);
            }

            ImGui::SameLine();
            ImGui::Text("%s", path.c_str());

            if (isSelected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();
    return changed;
}

// Draws a texture thumbnail (64x64 by default) with a fallback label.
inline void DrawTextureThumbnail(const Ref<Texture2D>& texture, float size = 64.0f) {
    if (texture && texture->getRendererID() != 0) {
        ImTextureID texID = (ImTextureID)(uintptr_t)texture->getRendererID();
        ImGui::Image(texID, ImVec2(size, size));
    } else {
        ImGui::Button("?", ImVec2(size, size));
    }
}

} // namespace AssetBrowser
} // namespace ve

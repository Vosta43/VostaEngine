#pragma once

#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

#include "Core/ResourceManager.h"
#include "Renderer/Texture.h"

namespace ve {
namespace assetvis {

// Card geometry shared by the File Browser and the Asset Manager, so a tile
// looks the same in both.
constexpr float kTile = 76.0f;
constexpr float kPad = 8.0f;
constexpr float kRounding = 6.0f;

// Name as shown in the browser: baked-asset extensions are noise, so they are
// hidden everywhere the name is displayed.
inline std::string displayName(const std::string& name) {
    for (const std::string ext : { std::string(".veasset"), std::string(".veworld") }) {
        if (name.size() > ext.size() &&
            name.compare(name.size() - ext.size(), ext.size(), ext) == 0)
            return name.substr(0, name.size() - ext.size());
    }
    return name;
}

// Wrap a name into at most two lines fitting maxWidth, breaking only on UTF-8
// character boundaries and ellipsising the second line if needed.
inline void wrapTwoLines(const std::string& text, float maxWidth,
                         std::string& line1, std::string& line2) {
    line1.clear();
    line2.clear();

    std::vector<std::string> glyphs;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
        if (len > text.size() - i) len = text.size() - i;
        glyphs.emplace_back(text.substr(i, len));
        i += len;
    }

    const auto fits = [maxWidth](const std::string& s) {
        return ImGui::CalcTextSize(s.c_str()).x <= maxWidth;
    };

    size_t i = 0;
    for (; i < glyphs.size(); ++i) {
        if (!fits(line1 + glyphs[i])) break;
        line1 += glyphs[i];
    }
    if (i >= glyphs.size())
        return;

    const std::string ellipsis = "\xE2\x80\xA6";   // U+2026
    for (; i < glyphs.size(); ++i) {
        if (!fits(line2 + glyphs[i] + ellipsis)) break;
        line2 += glyphs[i];
    }
    line2 += ellipsis;
}

// Hand the loaded texture straight to ImGui as its own icon. The
// ResourceManager caches it by path, so a repeat lookup is a map hit rather
// than a re-decode.
inline std::shared_ptr<Texture2D> texturePreviewForAsset(const std::string& assetPath) {
    AssetHandle handle = ResourceManager::find<Texture2D>(assetPath);
    if (!handle.isValid())
        handle = ResourceManager::store<Texture2D>(assetPath);
    if (!handle.isValid())
        return nullptr;
    return ResourceManager::get<Texture2D>(handle);
}

// Colour of a tile's separator rule, keyed by catalogue typeKey.
inline ImU32 accentColorForTypeKey(const std::string& typeKey) {
    const ImU32 kDefault = IM_COL32(130, 180, 235, 255);

    if (typeKey == "staticmesh")       return IM_COL32(70, 140, 245, 255);
    if (typeKey == "texture")          return IM_COL32(235, 125, 185, 255);
    if (typeKey == "material" ||
        typeKey == "layered_material" ||
        typeKey == "material_layer")   return IM_COL32(170, 125, 240, 255);
    if (typeKey == "noise")            return IM_COL32(90, 200, 170, 255);
    if (typeKey == "particle")         return IM_COL32(255, 175, 70, 255);
    if (typeKey == "geometry")         return IM_COL32(110, 150, 250, 255);
    if (typeKey == "terrain_data" ||
        typeKey == "terrain_rule")     return IM_COL32(110, 200, 120, 255);
    if (typeKey == "hud")              return IM_COL32(230, 210, 90, 255);
    if (typeKey == "sound")            return IM_COL32(235, 120, 120, 255);
    if (typeKey == "scene")            return IM_COL32(220, 220, 220, 255);
    return kDefault;
}

} // namespace assetvis
} // namespace ve

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Asset/AssetLibrary.h"
#include "Renderer/Texture.h"

namespace ve {

// Flat catalogue of every asset under the open project's content/ folder, drawn
// with the File Browser's card style. Search, category/type filters and sorting
// narrow a local view; double-click hands the asset path back to the host.
class AssetManagerPanel {
public:
    using OpenAssetCallback = std::function<void(const std::string& path)>;

    AssetManagerPanel();

    void setOnOpenAsset(OpenAssetCallback callback);

    // Rescan on the next render. Callers that change assets elsewhere use this
    // instead of refreshing now; an unrendered panel costs nothing.
    void requestRefresh();

    void onGuiRender(bool* openFlag = nullptr);

private:
    void refreshCatalogue();
    void rebuildTypeOptions();
    void rebuildView();
    std::shared_ptr<Texture2D> thumbnailFor(const AssetEntry& entry);
    void loadIcons();

    OpenAssetCallback m_onOpen;

    char m_search[128] = {};
    int m_categoryIndex = 0;   // 0 = all categories
    int m_typeIndex = 0;       // 0 = all types in the selected category
    int m_sortKey = 0;         // 0=name, 1=type, 2=category, 3=path
    bool m_sortAsc = true;
    bool m_refreshRequested = true;

    std::string m_selectedPath;

    std::vector<std::string> m_categories;
    std::vector<std::pair<std::string, std::string>> m_typeOptions;  // typeKey, displayName
    std::vector<const AssetEntry*> m_view;

    std::unordered_map<std::string, std::string> m_typeDisplayNames;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_thumbCache;

    std::shared_ptr<Texture2D> m_iconNoise;
    std::shared_ptr<Texture2D> m_iconParticle;
    std::shared_ptr<Texture2D> m_iconTerrainData;
    std::shared_ptr<Texture2D> m_iconTerrain;
    std::shared_ptr<Texture2D> m_iconGeometry;
    std::shared_ptr<Texture2D> m_iconHud;
    std::shared_ptr<Texture2D> m_iconSound;
    std::shared_ptr<Texture2D> m_iconMaterial;
    std::shared_ptr<Texture2D> m_iconFallback;
};

} // namespace ve

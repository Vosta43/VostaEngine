#include "Panels/AssetEditorHost.h"

#include "Asset/Utils.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"
#include "Geometry/GeometryGraphResource.h"
#include "Noise/NoiseGraphResource.h"
#include "Renderer/LayeredMaterial.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialLayerAsset.h"

#include <filesystem>

namespace ve {

namespace {

// The texture .veasset baked for a source image: same stem, or stem with an
// auto-suffix ("sofa_1"). "" when the source has not been imported.
std::string findBakedTexture(const std::string& sourcePath) {
    const std::filesystem::path src(sourcePath);
    const std::string stem = src.stem().string();

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(src.parent_path(), ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".veasset")
            continue;
        if (utils::peekAssetToken(entry.path().string()) != "texture")
            continue;

        const std::string cand = entry.path().stem().string();
        if (cand == stem || cand.rfind(stem + "_", 0) == 0)
            return entry.path().string();
    }
    return "";
}

}

AssetEditorHost::AssetEditorHost() {
    // A layer stack opens the individual layer assets in their own window.
    m_layeredMaterialEditor.setOnOpenLayer(
        [this](AssetHandle handle) { openMaterialLayer(handle); });
}

void AssetEditorHost::openMaterial(AssetHandle handle) {
    if (!handle.isValid()) return;

    Ref<Material> material = ResourceManager::get<Material>(handle);

    // A layered material has no node graph: the stack editor edits it instead.
    if (std::dynamic_pointer_cast<LayeredMaterial>(material)) {
        openLayeredMaterial(handle);
        return;
    }

    // Otherwise the graph editor only edits the single-surface kind.
    m_openMaterial = std::dynamic_pointer_cast<SingleMaterial>(material);
    if (!m_openMaterial) return;

    m_openMaterialOriginalHandle = handle;
    m_showMaterialEditor = true;
}

void AssetEditorHost::openLayeredMaterial(AssetHandle handle) {
    if (!handle.isValid()) return;
    m_layeredMaterialEditor.openHandle(handle);
    m_showLayeredMaterialEditor = true;
}

void AssetEditorHost::openMaterialLayer(AssetHandle handle) {
    if (!handle.isValid()) return;

    const std::string rel = ResourceManager::getPath<MaterialLayerAsset>(handle);
    if (rel.empty()) return;
    m_materialLayerEditor.openFile(toAbsolute(rel));
    m_showMaterialLayerEditor = true;
}

void AssetEditorHost::openNoise(const std::string& path) {
    // Register with ResourceManager so the terrain's Noise Graph combo can list
    // it; NoisePanel keeps its own copy for editing.
    if (!ResourceManager::find<NoiseGraphResource>(path).isValid())
        ResourceManager::store<NoiseGraphResource>(path);
    m_noisePanel.openFile(path);
    m_showNoiseEditor = true;
}

void AssetEditorHost::openGeometry(const std::string& path) {
    // Register with ResourceManager so the asset is loadable like any other graph;
    // GeometryGraphPanel keeps its own copy for editing.
    if (!ResourceManager::find<GeometryGraphResource>(path).isValid())
        ResourceManager::store<GeometryGraphResource>(path);
    m_geometryPanel.openFile(path);
    m_showGeometryEditor = true;
}

void AssetEditorHost::openTerrainRule(const std::string& path) {
    // Register with ResourceManager so the terrain inspector's rule-graph combo can
    // list it; TerrainRulePanel keeps its own copy for editing.
    if (!ResourceManager::find<TerrainRuleGraphResource>(path).isValid())
        ResourceManager::store<TerrainRuleGraphResource>(path);
    m_terrainRulePanel.openFile(path);
    m_showTerrainRuleEditor = true;
}

void AssetEditorHost::openTexture(const std::string& path) {
    AssetHandle handle = ResourceManager::find<Texture2D>(path);
    if (!handle.isValid())
        handle = ResourceManager::store<Texture2D>(path);

    auto tex = ResourceManager::get<Texture2D>(handle);
    if (!tex)
        return;

    m_texturePreview.open(tex, std::filesystem::path(path).filename().string());
    m_showTexturePreview = true;
}

bool AssetEditorHost::openFile(const std::string& path) {
    const std::string ext = utils::getExtension(path);

    if (ext == ".veasset") {
        const std::string token = utils::peekAssetToken(path);

        if (token == "material" || token == "layered_material") {
            // A layered material has no node graph: the layer stack gets its own
            // editor window, so it opens the same way a single-surface one does.
            AssetHandle handle = ResourceManager::find<Material>(path);
            if (!handle.isValid())
                handle = ResourceManager::store<Material>(path);
            if (token == "material")
                openMaterial(handle);
            else
                openLayeredMaterial(handle);
            return true;
        }

        if (token == "material_layer") {
            // A shared material layer: register it so material inspectors can list
            // it, then open it for editing.
            if (!ResourceManager::find<MaterialLayerAsset>(path).isValid())
                ResourceManager::store<MaterialLayerAsset>(path);
            m_materialLayerEditor.openFile(path);
            m_showMaterialLayerEditor = true;
            return true;
        }

        if (token == "noise") {
            openNoise(path);
            return true;
        }

        if (token == "geometry") {
            openGeometry(path);
            return true;
        }

        if (token == "terrain_rule") {
            openTerrainRule(path);
            return true;
        }

        if (token == "texture") {
            openTexture(path);
            return true;
        }

        return false;
    }

    // Double-clicking a source image previews its baked texture, so the browser's
    // image tile works as well as the .veasset tile.
    if (ext == ".png" || ext == ".jpg") {
        const std::string baked = findBakedTexture(path);
        if (baked.empty())
            VE_CORE_WARN_PRINT("No imported texture for %s", path.c_str());
        else
            openTexture(baked);
        return true;
    }

    return false;
}

void AssetEditorHost::onGuiRender() {
    if (m_showMaterialEditor && m_openMaterial) {
        m_materialGraphPanel.onGuiRender(m_openMaterial, m_needsMaterialRecompile,
                                         &m_showMaterialEditor, m_openMaterialOriginalHandle);
    }

    if (m_showNoiseEditor) {
        m_noisePanel.onGuiRender(&m_showNoiseEditor);
    }

    if (m_showGeometryEditor) {
        m_geometryPanel.onGuiRender(&m_showGeometryEditor);
    }

    if (m_showTerrainRuleEditor) {
        m_terrainRulePanel.onGuiRender(&m_showTerrainRuleEditor);
    }

    if (m_showMaterialLayerEditor) {
        m_materialLayerEditor.onGuiRender(&m_showMaterialLayerEditor);
    }

    if (m_showLayeredMaterialEditor) {
        m_layeredMaterialEditor.onGuiRender(&m_showLayeredMaterialEditor);
    }

    if (m_showTexturePreview) {
        m_texturePreview.onGuiRender(&m_showTexturePreview);
    }
}

}

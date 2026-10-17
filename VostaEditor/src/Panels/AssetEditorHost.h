#pragma once

#include <VostaEngine.h>

#include "Panels/GeometryGraphPanel.h"
#include "Panels/MaterialGraphPanel.h"
#include "Panels/NoisePanel.h"
#include "Panels/TerrainRulePanel.h"
#include "Panels/TexturePreviewPanel.h"
#include "SubEditors/LayeredMaterialEditor.h"
#include "SubEditors/MaterialLayerEditor.h"

#include <string>

namespace ve {

// Hosts the asset editors that open onto a .veasset (or a source image whose
// texture bake exists): the single-surface material graph, the layered-material
// layer stack, a shared material layer, the noise graph, and the texture viewer.
// Owns the panels and their window flags, and routes a double-clicked file to the
// right one.
class AssetEditorHost {
public:
    AssetEditorHost();

    // Opens whatever this host owns for `path`. False when the path is not one of
    // its kinds -- the caller handles a .veworld itself.
    bool openFile(const std::string& path);

    void openMaterial(AssetHandle handle);
    void openMaterialLayer(AssetHandle handle);
    void openLayeredMaterial(AssetHandle handle);
    void openNoise(const std::string& path);
    void openGeometry(const std::string& path);
    void openTerrainRule(const std::string& path);
    void openTexture(const std::string& path);

    // Shows the noise editor window on whatever graph it last held.
    void showNoiseEditor() { m_showNoiseEditor = true; }

    // Shows the geometry editor window on whatever graph it last held.
    void showGeometryEditor() { m_showGeometryEditor = true; }

    // Draws every open window.
    void onGuiRender();

private:
    MaterialGraphPanel    m_materialGraphPanel;
    NoisePanel            m_noisePanel;
    GeometryGraphPanel    m_geometryPanel;
    TerrainRulePanel      m_terrainRulePanel;
    MaterialLayerEditor   m_materialLayerEditor;
    LayeredMaterialEditor m_layeredMaterialEditor;
    TexturePreviewPanel   m_texturePreview;

    Ref<SingleMaterial> m_openMaterial = nullptr;
    AssetHandle m_openMaterialOriginalHandle;
    bool m_needsMaterialRecompile = false;

    bool m_showMaterialEditor = false;
    bool m_showNoiseEditor = false;
    bool m_showGeometryEditor = false;
    bool m_showTerrainRuleEditor = false;
    bool m_showMaterialLayerEditor = false;
    bool m_showLayeredMaterialEditor = false;
    bool m_showTexturePreview = false;
};

}

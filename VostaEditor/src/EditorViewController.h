#pragma once
#include <VostaEngine.h>

#include "Panels/SceneOutlinerPanel.h"
#include "Panels/PropertyPanel.h"

#include <functional>

namespace ve {

// Owns the scene and its editor-facing UI panels (outliner, property panel,
// scene renderer). Eliminates the duplication between EditorLayer's constructor
// and loadScene() by providing a single rebind() entry point.
class EditorViewController {
public:
    using OpenMaterialEditorFn = std::function<void(AssetHandle)>;
    using TerrainBrushFn = std::function<void(Scene&)>;
    using OpenTerrainMapFn = std::function<void()>;

    EditorViewController(Ref<Scene> scene, OpenMaterialEditorFn onOpenMaterial,
                         OpenMaterialEditorFn onOpenMaterialLayer, TerrainBrushFn onDrawTerrainBrush,
                         OpenTerrainMapFn onOpenTerrainMap);

    // Switch to a new scene (e.g. after loading a file).
    // Recreates all dependent panels and the scene renderer.
    void rebind(Ref<Scene> newScene, OpenMaterialEditorFn onOpenMaterial,
                OpenMaterialEditorFn onOpenMaterialLayer, TerrainBrushFn onDrawTerrainBrush,
                OpenTerrainMapFn onOpenTerrainMap);

    // Renders the outliner and property panels.
    void onGuiRender(uint32_t& selectedEntity);

    Ref<Scene>             getScene()       { return m_scene; }
    Ref<SceneViewRenderer> getViewRenderer() { return m_viewRenderer; }

private:
    Ref<Scene>               m_scene;
    SceneOutliner            m_outliner;
    PropertyPanel            m_propertyPanel;
    Ref<SceneViewRenderer>   m_viewRenderer;
};

} // namespace ve

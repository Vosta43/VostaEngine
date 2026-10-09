#include "EditorViewController.h"

namespace ve {

EditorViewController::EditorViewController(Ref<Scene> scene, OpenMaterialEditorFn onOpenMaterial,
                                           OpenMaterialEditorFn onOpenMaterialLayer,
                                           TerrainBrushFn onDrawTerrainBrush,
                                           OpenTerrainMapFn onOpenTerrainMap)
    : m_scene(std::move(scene))
    , m_outliner(m_scene)
    , m_propertyPanel(m_scene)
    , m_viewRenderer(CreateRef<SceneViewRenderer>(m_scene))
{
    m_propertyPanel.setOnOpenMaterialEditor(std::move(onOpenMaterial));
    m_propertyPanel.setOnOpenMaterialLayerEditor(std::move(onOpenMaterialLayer));
    m_propertyPanel.setOnDrawTerrainBrush(std::move(onDrawTerrainBrush));
    m_propertyPanel.setOnOpenTerrainMap(std::move(onOpenTerrainMap));
}

void EditorViewController::rebind(Ref<Scene> newScene, OpenMaterialEditorFn onOpenMaterial,
                                  OpenMaterialEditorFn onOpenMaterialLayer,
                                  TerrainBrushFn onDrawTerrainBrush,
                                  OpenTerrainMapFn onOpenTerrainMap)
{
    m_scene         = std::move(newScene);
    m_outliner      = SceneOutliner(m_scene);
    m_propertyPanel = PropertyPanel(m_scene);
    m_propertyPanel.setOnOpenMaterialEditor(std::move(onOpenMaterial));
    m_propertyPanel.setOnOpenMaterialLayerEditor(std::move(onOpenMaterialLayer));
    m_propertyPanel.setOnDrawTerrainBrush(std::move(onDrawTerrainBrush));
    m_propertyPanel.setOnOpenTerrainMap(std::move(onOpenTerrainMap));
    m_viewRenderer = CreateRef<SceneViewRenderer>(m_scene);
}

void EditorViewController::onGuiRender(uint32_t& selectedEntity)
{
    m_outliner.onGuiRender(selectedEntity);
    m_propertyPanel.onGuiRender(selectedEntity);
}

} // namespace ve

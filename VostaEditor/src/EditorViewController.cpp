#include "EditorViewController.h"

namespace ve {

EditorViewController::EditorViewController(Ref<Scene> scene, OpenMaterialEditorFn onOpenMaterial)
    : m_scene(std::move(scene))
    , m_outliner(m_scene)
    , m_propertyPanel(m_scene)
    , m_viewRenderer(CreateRef<SceneViewRenderer>(m_scene))
{
    m_propertyPanel.setOnOpenMaterialEditor(std::move(onOpenMaterial));
    
    // TODO: Remove the hard-coded system initialize
    auto renderSystem = CreateRef<RenderSystem>(m_scene);
    m_scene->addSystem(renderSystem);
}

void EditorViewController::rebind(Ref<Scene> newScene, OpenMaterialEditorFn onOpenMaterial)
{
    m_scene         = std::move(newScene);
    m_outliner      = SceneOutliner(m_scene);
    m_propertyPanel = PropertyPanel(m_scene);
    m_propertyPanel.setOnOpenMaterialEditor(std::move(onOpenMaterial));
    m_viewRenderer = CreateRef<SceneViewRenderer>(m_scene);
}

void EditorViewController::onGuiRender(uint32_t& selectedEntity)
{
    m_outliner.onGuiRender(selectedEntity);
    m_propertyPanel.onGuiRender(selectedEntity);
}

} // namespace ve

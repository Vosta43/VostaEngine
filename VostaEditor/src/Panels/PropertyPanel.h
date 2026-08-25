#pragma once
#include <VostaEngine.h>

#include <functional>

namespace ve {

class PropertyPanel {
public:
    PropertyPanel() = default;
    PropertyPanel(const Ref<Scene>& sceneContext);

    void onGuiRender(uint32_t selectedEntity);

    // Callback invoked when the user clicks "Open Editor" next to a material.
    using OpenMaterialEditorFn = std::function<void(AssetHandle)>;
    void setOnOpenMaterialEditor(OpenMaterialEditorFn callback) { m_onOpenMaterialEditor = std::move(callback); }

private:
    // Dispatch drawProperty by uiType (extracted from the old if-else chain).
    void drawProperty(const std::string& typeName, void* compPtr, const ReflectionProperty& prop);

    // Per-uiType drawers.
    void drawDragProperty(const std::string& typeName, void* compPtr, const ReflectionProperty& prop);
    void drawInputProperty(void* ptr, const ReflectionProperty& prop);
    void drawMaterialProperty(void* ptr, const ReflectionProperty& prop);
    void drawTextureProperty(void* ptr);
    void drawSkyboxTextureProperty(void* ptr);

    // Component-level custom drawers.
    void drawStaticMeshComponent(StaticMeshComponent* comp);
    void drawTerrainComponent(uint32_t entityId, TerrainComponent* comp);

    // Shared: texture selector with preview, search combo, and clear button.
    void drawTexturePropertyWidget(const std::string& label, AssetHandle& textureHandle, AssetHandle materialHandle);

    Ref<Scene> m_sceneContext;
    OpenMaterialEditorFn m_onOpenMaterialEditor;
    glm::vec3 m_editPosition = glm::vec3(0.0f);
    glm::vec3 m_editRotation = glm::vec3(0.0f);
    glm::vec3 m_editScale = glm::vec3(1.0f);
    char m_nameBuffer[256] = "";

    // "Add Component" popup state.
    bool m_showAddComponentPopup = false;
    char m_componentSearch[256] = "";
};

} // namespace ve

#pragma once
#include <VostaEngine.h>

#include <functional>

namespace ve {

class PropertyPanel {
public:
    PropertyPanel() = default;
    PropertyPanel(const Ref<Scene>& sceneContext);

    // Selection is by reference so a tile-neighbour button can jump to another
    // terrain, the same way the outliner does.
    void onGuiRender(uint32_t& selectedEntity);

    // Callback invoked when the user clicks "Open Editor" next to a material.
    using OpenMaterialEditorFn = std::function<void(AssetHandle)>;
    void setOnOpenMaterialEditor(OpenMaterialEditorFn callback) { m_onOpenMaterialEditor = std::move(callback); }
    // Callback invoked when the user clicks "Edit" next to a material layer.
    void setOnOpenMaterialLayerEditor(OpenMaterialEditorFn callback) { m_onOpenMaterialLayerEditor = std::move(callback); }

    // Callback invoked when the user clicks "Open Terrain Map" in the terrain
    // system inspector.
    using OpenTerrainMapFn = std::function<void()>;
    void setOnOpenTerrainMap(OpenTerrainMapFn callback) { m_onOpenTerrainMap = std::move(callback); }

    // Draws the terrain paint-brush section inline in the TerrainSystem block.
    // The brush spans the whole terrain, so it needs no tile selection. Its state
    // lives with the editor layer (which also consumes it for viewport input), so
    // this is a draw callback, not a state handoff.
    using TerrainBrushFn = std::function<void(Scene&)>;
    void setOnDrawTerrainBrush(TerrainBrushFn callback) { m_onDrawTerrainBrush = std::move(callback); }

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
    void drawTerrainComponent(uint32_t& entityId, TerrainComponent* comp);
    // The scene's shared terrain parameters: source, material, tile scale and
    // LOD. One per scene, so every edit re-bakes and re-places all tiles.
    void drawTerrainSystemComponent(TerrainSystemComponent* comp);
    void drawTerrainTiles(uint32_t& entityId, TerrainComponent* comp);

    // Re-bake after the user picks a terrain source, then reconcile the tile with
    // the grid so its coordinate matches where it actually sits.
    void applyTerrainSource(uint32_t entityId, TerrainComponent* comp);

    // Shared: texture selector with preview, search combo, and clear button.
    void drawTexturePropertyWidget(const std::string& label, AssetHandle& textureHandle, AssetHandle materialHandle);

    Ref<Scene> m_sceneContext;
    OpenMaterialEditorFn m_onOpenMaterialEditor;
    OpenMaterialEditorFn m_onOpenMaterialLayerEditor;
    OpenTerrainMapFn m_onOpenTerrainMap;
    TerrainBrushFn m_onDrawTerrainBrush;
    glm::vec3 m_editPosition = glm::vec3(0.0f);
    glm::vec3 m_editRotation = glm::vec3(0.0f);
    glm::vec3 m_editScale = glm::vec3(1.0f);
    char m_nameBuffer[256] = "";

    // A tile edit spawns or destroys an entity, which reallocates the component
    // storage that this frame's component pointers were taken from. The request is
    // held for one frame and applied before the next snapshot is taken.
    enum class PendingTileOp { None, Add, Remove, EnsureSystem };
    PendingTileOp m_pendingTileOp = PendingTileOp::None;
    glm::ivec2 m_pendingTileCoord = glm::ivec2(0);

    // "Add Component" popup state.
    bool m_showAddComponentPopup = false;
    char m_componentSearch[256] = "";
};

} // namespace ve

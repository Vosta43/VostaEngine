#pragma once
#include <VostaEngine.h>

#include <cstdint>

namespace ve {

class Scene;

// Bird's-eye 2D map of the terrain grid: one square per tile coordinate, filled
// when a tile entity exists there. Whole regions can be laid out or cleared at
// once through Terrain::addTiles/removeTiles, which is far quicker than the
// per-tile neighbour buttons in the inspector.
class TerrainMapPanel {
public:
    // No stored scene: the caller passes it every frame (like TerrainEditor), so
    // switching scenes needs no rebinding.
    void onGuiRender(Scene& scene, bool* openFlag = nullptr);

private:
    // View transform: cell (0,0) centre sits at the canvas centre plus m_pan.
    float     m_zoom = 16.0f;                 // pixels per cell
    glm::vec2 m_pan = glm::vec2(0.0f);

    // Selection rectangle in cell coordinates, inclusive. The drag leaves it in
    // place; only a new click/drag or Escape replaces it.
    bool      m_hasSelection = false;
    glm::ivec2 m_selMin = glm::ivec2(0);
    glm::ivec2 m_selMax = glm::ivec2(0);

    bool      m_dragging = false;
    glm::ivec2 m_dragStart = glm::ivec2(0);

    // Right-click target, captured when the popup is requested so the menu acts
    // on the region that was under the cursor.
    glm::ivec2 m_popupMin = glm::ivec2(0);
    glm::ivec2 m_popupMax = glm::ivec2(0);
    bool       m_openPopup = false;
};

} // namespace ve

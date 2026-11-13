#pragma once
#include <VostaEngine.h>

#include <functional>
#include <utility>

namespace ve {

class SceneOutliner {
public:
    // Where a newly created entity should land, in chart space. Queried on the
    // click, so it can follow the viewport the host is showing.
    using SpawnPlacementFn = std::function<ve::Vec3()>;

    SceneOutliner() = default;
    SceneOutliner(const Ref<Scene>& sceneContext);

    // Unset = the prefab's own transform (the origin for most templates).
    void setSpawnPlacement(SpawnPlacementFn fn) { m_spawnPlacement = std::move(fn); }

    // Renders the entity list and create-entity popup.
    // selectedEntity is read/write — the user can click an entity to select it.
    void onGuiRender(uint32_t& selectedEntity);

    // While locked the panel renders no editing UI (a play session is live).
    void setLocked(bool locked) { m_locked = locked; }

private:
    Ref<Scene> m_sceneContext;
    SpawnPlacementFn m_spawnPlacement;
    bool m_showEntityCreatePopup = false;
    bool m_locked = false;
    char m_prefabSearch[64] = {};
};

} // namespace ve

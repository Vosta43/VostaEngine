#pragma once
#include <VostaEngine.h>

namespace ve {

class SceneOutliner {
public:
    SceneOutliner() = default;
    SceneOutliner(const Ref<Scene>& sceneContext);

    // Renders the entity list and create-entity popup.
    // selectedEntity is read/write — the user can click an entity to select it.
    void onGuiRender(uint32_t& selectedEntity);

    // While locked the panel renders no editing UI (a play session is live).
    void setLocked(bool locked) { m_locked = locked; }

private:
    Ref<Scene> m_sceneContext;
    bool m_showEntityCreatePopup = false;
    bool m_locked = false;
};

} // namespace ve

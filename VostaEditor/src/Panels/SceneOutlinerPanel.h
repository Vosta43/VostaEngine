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

private:
    Ref<Scene> m_sceneContext;
    bool m_showEntityCreatePopup = false;
};

} // namespace ve

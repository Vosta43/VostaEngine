#include "SceneOutlinerPanel.h"

#include "imgui.h"

namespace ve {

SceneOutliner::SceneOutliner(const Ref<Scene>& sceneContext)
    : m_sceneContext(sceneContext)
{
}

void SceneOutliner::onGuiRender(uint32_t& selectedEntity)
{
    ImGui::Begin("Scene Outliner");

    ImVec2 windowSize = ImGui::GetContentRegionAvail();
    float buttonHeight = ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("EntityList", ImVec2(windowSize.x, windowSize.y - buttonHeight), false);
    {
        auto allEntities = m_sceneContext->getRegistry().each();

        for (auto entityId : allEntities) {
            ImGui::PushID(static_cast<int>(entityId));

            std::string label;
            if (m_sceneContext->getRegistry().has<NameComponent>(entityId)) {
                label = m_sceneContext->getRegistry().get<NameComponent>(entityId).name;
            } else {
                label = "Unnamed Entity";
            }

            bool isSelected = (entityId == selectedEntity);
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                selectedEntity = entityId;
            }

            // Right-clicking an entity also selects it, then opens its context menu.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                selectedEntity = entityId;
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    Entity e = m_sceneContext->getEntity(entityId);
                    m_sceneContext->destroyEntity(e);
                    if (entityId == selectedEntity)
                        selectedEntity = 0xFFFFFFFF;
                }
                if (ImGui::MenuItem("Copy")) {
                    Entity e = m_sceneContext->getEntity(entityId);
                    Entity copy = m_sceneContext->duplicateEntity(e);
                    selectedEntity = copy.m_id;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("Create Entity", ImVec2(-1, 0))) {
        m_showEntityCreatePopup = true;
    }

    if (m_showEntityCreatePopup) {
        ImVec2 popupSize(200, 250);
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(popupSize, ImGuiCond_Appearing);
        ImGui::OpenPopup("Create Entity");
        if (ImGui::BeginPopupModal("Create Entity", &m_showEntityCreatePopup, ImGuiWindowFlags_NoResize)) {
            // One button per registered prefab, so the editor and the agent spawn
            // from the same templates. The scroll child keeps the list usable as it
            // grows past the fixed modal size.
            ImGui::BeginChild("##prefabs");
            for (const PrefabInfo& prefab : PrefabRegistry::get().list()) {
                if (ImGui::Button(prefab.displayName.c_str(), ImVec2(-1.0f, 0.0f))) {
                    Entity spawned = PrefabRegistry::get().spawn(prefab.key, *m_sceneContext);
                    if (spawned.getId() != 0xFFFFFFFFu)
                        selectedEntity = spawned.getId();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

} // namespace ve

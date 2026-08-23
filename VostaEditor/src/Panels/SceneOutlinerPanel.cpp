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
            if (ImGui::Button("Light")) {
                Entity newEntity = m_sceneContext->createEntity();
                m_sceneContext->assignComponent<NameComponent>(newEntity, "Light");
                m_sceneContext->assignComponent<TransformComponent>(newEntity, glm::mat4(1.0f));
                m_sceneContext->assignComponent<LightComponent>(newEntity);
                selectedEntity = newEntity.m_id;
            }
            if (ImGui::Button("Static Mesh")) {
                Entity newEntity = m_sceneContext->createEntity();
                m_sceneContext->assignComponent<NameComponent>(newEntity, "Mesh");
                m_sceneContext->assignComponent<TransformComponent>(newEntity, glm::mat4(1.0f));
                m_sceneContext->assignComponent<StaticMeshComponent>(newEntity);
                selectedEntity = newEntity.m_id;
            }
            if (ImGui::Button("Skybox")) {
                Entity newEntity = m_sceneContext->createEntity();
                m_sceneContext->assignComponent<NameComponent>(newEntity, "Skybox");
                m_sceneContext->assignComponent<TransformComponent>(newEntity, glm::mat4(1.0f));
                m_sceneContext->assignComponent<SkyBoxComponent>(newEntity);
                selectedEntity = newEntity.m_id;
            }
            if (ImGui::Button("Atmosphere")) {
                Entity newEntity = m_sceneContext->createEntity();
                m_sceneContext->assignComponent<NameComponent>(newEntity, "Atmosphere");

                // Planet centre sits directly below the scene origin, so y = 0 is the surface.
                float planetRadius = AtmosphereComponent().atmosphere.planetRadius;
                glm::mat4 transform(1.0f);
                transform[3] = glm::vec4(0.0f, -planetRadius, 0.0f, 1.0f);
                m_sceneContext->assignComponent<TransformComponent>(newEntity, transform);

                m_sceneContext->assignComponent<AtmosphereComponent>(newEntity);
                selectedEntity = newEntity.m_id;
            }
            if (ImGui::Button("Sprite")) {
                Entity newEntity = m_sceneContext->createEntity();
                m_sceneContext->assignComponent<NameComponent>(newEntity, "Sprite");
                m_sceneContext->assignComponent<TransformComponent>(newEntity, glm::mat4(1.0f));
                m_sceneContext->assignComponent<SpriteRendererComponent>(newEntity);
                selectedEntity = newEntity.m_id;
            }
            if (ImGui::Button("Terrain")) {
                Entity newEntity = m_sceneContext->createEntity();
                m_sceneContext->assignComponent<NameComponent>(newEntity, "Terrain");
                m_sceneContext->assignComponent<TransformComponent>(newEntity, glm::mat4(1.0f));
                m_sceneContext->assignComponent<TerrainComponent>(newEntity);
                selectedEntity = newEntity.m_id;
            }

            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

} // namespace ve

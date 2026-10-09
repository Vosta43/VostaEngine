#pragma once

#include "Core/Core.h"
#include "Core/Deltatime.h"

#include "EntityRegistry.h"
#include "Core/ResourceManager.h"
#include "Renderer/RenderContext.h"
#include "ECSystems/ECSystemGraph.h"

#include <glm.hpp>
#include <typeindex>

namespace ve {
	class VE_API Scene {
	public:
		Scene();
		~Scene();

		void onUpdate(float deltaTime);

        void onPickingRender();

        Entity createEntity();
		
        void destroyEntity(Entity entity) {
            m_entityRegistry.destroy(entity);
        }

        Entity duplicateEntity(Entity entity) {
            return m_entityRegistry.duplicate(entity);
        }

        Entity getEntity(uint32_t entityId) {
            return m_entityRegistry.getEntity(entityId);
        }

        template<typename T, typename... Args>
        T& assignComponent(Entity entity, Args&&... args) {
            T& comp = m_entityRegistry.emplace<T>(entity, std::forward<Args>(args)...);
            onComponentAssigned(std::type_index(typeid(T)), entity);
            return comp;
        }

        template<typename T>
        void removeComponent(Entity entity) {
            m_entityRegistry.removeComponent<T>(entity);
        }

        template<typename T>
        T& getComponent(Entity entity) {
            return m_entityRegistry.get<T>(entity);
        }

        template<typename T>
        T& getComponent(uint32_t entityId) {
            return m_entityRegistry.get<T>(entityId);
        }

        template<typename T>
        auto eachComponent() {
            return m_entityRegistry.view<T>();
        }

        template<typename... Components>
        auto eachComponent() {
            return m_entityRegistry.group<Components...>();
        }

        void clearAllEntity() {
            m_entityRegistry.clearAllEntity();
        }

        void addSystem(const Ref<ECSystemBase>& system) {
            m_ecsystemGraph.addSystem(system);
        }


        EntityRegistry& getRegistry() { return m_entityRegistry; }
        const EntityRegistry& getRegistry() const { return m_entityRegistry; }
        
        void setCameraMatrices(const glm::mat4& view, const glm::mat4& proj);
        void setCameraPosition(const glm::vec3& pos);

        const glm::mat4& getViewMatrix() {return m_viewMatrix;}
        const glm::mat4& getProjMatrix() { return m_projMatrix; }
        const glm::vec3& getCameraPosition() {return m_cameraPosition;}

    private:
        // Runs the component type's init hook (if any) after it is assigned.
        // Defined in Scene.cpp so this header stays free of the component
        // registry's JSON dependency.
        void onComponentAssigned(std::type_index type, Entity entity);

        // CameraController is at Editor exe. Intermediate variable
        glm::vec3 m_cameraPosition;
        glm::mat4 m_viewMatrix = glm::mat4(1.0f);
        glm::mat4 m_projMatrix = glm::mat4(1.0f);
		
        EntityRegistry m_entityRegistry;
        ECSytemGraph m_ecsystemGraph;
	};




}
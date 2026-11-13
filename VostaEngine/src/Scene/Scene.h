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
	class PhysicsWorld;

	class VE_API Scene {
	public:
		Scene();
		~Scene();

		void onUpdate(float deltaTime);

		// Runs exactly one fixed simulation step, bypassing the accumulator.
		// For paused single-stepping, where a sub-step delay must still tick.
		void stepFixed();

        // Advances particle components only. The editor calls this for the authored
        // scene while editing, where the ECS graph must not run.
        void tickParticles(float deltaTime);

        // Audio follows the primary camera and runs source components. Only the
        // runtime scene calls these; the authored scene stays silent in the
        // editor.
        void updateAudioListener();
        void tickAudio();
        void stopAudio();

        // Gate for the two calls above. Several runtime worlds share one mixer
        // (multi-world PIE), so only the focused world advances audio -- otherwise
        // every world's listener fights over one device and all sounds stack.
        void setAudioEnabled(bool enabled) { m_audioEnabled = enabled; }
        bool isAudioEnabled() const { return m_audioEnabled; }

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

        // The entity holding the primary CameraComponent (Unity's Camera.main).
        // Falls back to any camera when none is flagged primary; returns an
        // invalid handle (id 0xFFFFFFFF) when the scene has no camera at all.
        Entity getPrimaryCameraEntity();

        // Null when the scene has no RigidBodyComponent; otherwise the world is
        // created on first use with its bodies already in place.
        PhysicsWorld* getPhysics();

        void addSystem(const Ref<ECSystemBase>& system) {
            m_ecsystemGraph.addSystem(system);
        }

        void removeSystem(const Ref<ECSystemBase>& system) {
            m_ecsystemGraph.removeSystem(system);
        }


        EntityRegistry& getRegistry() { return m_entityRegistry; }
        const EntityRegistry& getRegistry() const { return m_entityRegistry; }
        
        void setCameraMatrices(const glm::mat4& view, const glm::mat4& proj);
        void setCameraPosition(const ve::Vec3& pos);

        const glm::mat4& getViewMatrix() {return m_viewMatrix;}
        const glm::mat4& getProjMatrix() { return m_projMatrix; }
        const ve::Vec3& getCameraPosition() {return m_cameraPosition;}

    private:
        // Runs the component type's init hook (if any) after it is assigned.
        // Defined in Scene.cpp so this header stays free of the component
        // registry's JSON dependency.
        void onComponentAssigned(std::type_index type, Entity entity);

        void ensurePhysicsWorld();
        void tickPhysics(float deltaTime);
        void tickFixedStep(float fixedDt);

        // Leftover frame time in fixed-step units; see Scene::onUpdate.
        double m_fixedAccumulator = 0.0;

        bool m_audioEnabled = true;

        // CameraController is at Editor exe. Intermediate variable
        ve::Vec3 m_cameraPosition{ 0.0 };
        glm::mat4 m_viewMatrix = glm::mat4(1.0f);
        glm::mat4 m_projMatrix = glm::mat4(1.0f);

        // Declared before the registry so components are destroyed first.
        Scope<PhysicsWorld> m_physicsWorld;

        EntityRegistry m_entityRegistry;
        ECSytemGraph m_ecsystemGraph;
	};




}
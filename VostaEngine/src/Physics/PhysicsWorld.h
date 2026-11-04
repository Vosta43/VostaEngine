#pragma once

#include "Core/Core.h"
#include "Core/Types.h"

#include <glm.hpp>
#include <gtc/quaternion.hpp>

#include <cstdint>

namespace ve {

	class EntityRegistry;

	// One Jolt world per scene. Jolt types stay behind the pimpl: Components.h is
	// included by game modules, and the Jolt headers must not cross that boundary.
	class VE_API PhysicsWorld {
	public:
		// Bodies are simulated relative to inOrigin so a large-world scene keeps
		// float precision near the play area.
		explicit PhysicsWorld(const Vec3& inOrigin = Vec3(0.0));
		~PhysicsWorld();

		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;

		// Reconciles bodies with the registry, advances fixed steps and writes
		// dynamic transforms back.
		void step(EntityRegistry& registry, float deltaTime);

		// Creates/removes bodies to match the registry without simulating.
		void syncBodies(EntityRegistry& registry);

		void setGravity(const glm::vec3& gravity);

		// Dynamic bodies only: kinematic and static bodies are driven by their
		// TransformComponent.
		void applyImpulse(uint32_t entityId, const glm::vec3& impulse);
		void setLinearVelocity(uint32_t entityId, const glm::vec3& velocity);
		glm::vec3 getLinearVelocity(uint32_t entityId) const;
		void teleport(uint32_t entityId, const Vec3& position, const glm::quat& rotation);

		bool hasBody(uint32_t entityId) const;

	private:
		struct Impl;
		Scope<Impl> m_impl;
	};

}

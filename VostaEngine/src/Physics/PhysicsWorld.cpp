#include "vepch.h"
#include "PhysicsWorld.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/EntityRegistry.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <unordered_map>

namespace ve {

	namespace {

		constexpr float kFixedStep = 1.0f / 60.0f;
		constexpr int kMaxSubSteps = 4;
		constexpr float kMaxFrameDelta = 0.1f;

		namespace Layers {
			constexpr JPH::ObjectLayer NonMoving = 0;
			constexpr JPH::ObjectLayer Moving = 1;
			constexpr JPH::uint Count = 2;
		}

		namespace BroadPhaseLayers {
			const JPH::BroadPhaseLayer NonMoving{ 0 };
			const JPH::BroadPhaseLayer Moving{ 1 };
			constexpr JPH::uint Count = 2;
		}

		// Base-class construction guarantees the Jolt allocator/factory exist
		// before any member that allocates through them.
		struct JoltBoot {
			JoltBoot() {
				static const bool initialized = [] {
					JPH::RegisterDefaultAllocator();
					JPH::Factory::sInstance = new JPH::Factory();
					JPH::RegisterTypes();
					return true;
				}();
				(void)initialized;
			}
		};

		// Jolt's layer interfaces are non-copyable, and
		// ObjectVsBroadPhaseLayerFilterTable snapshots both tables when it is
		// constructed. Configuring them in a base class guarantees that happens
		// before the derived member built from them, which a constructor body
		// cannot do.
		struct LayerSetup {
			LayerSetup() {
				broadPhaseLayerInterface.MapObjectToBroadPhaseLayer(Layers::NonMoving, BroadPhaseLayers::NonMoving);
				broadPhaseLayerInterface.MapObjectToBroadPhaseLayer(Layers::Moving, BroadPhaseLayers::Moving);
				objectLayerPairFilter.EnableCollision(Layers::NonMoving, Layers::Moving);
				objectLayerPairFilter.EnableCollision(Layers::Moving, Layers::Moving);
			}

			JPH::BroadPhaseLayerInterfaceTable broadPhaseLayerInterface{ Layers::Count, BroadPhaseLayers::Count };
			JPH::ObjectLayerPairFilterTable objectLayerPairFilter{ Layers::Count };
		};

		JPH::RefConst<JPH::Shape> makeShape(const RigidBodyComponent& component) {
			const float radius = std::max(component.radius, 0.001f);
			switch (component.shape) {
			case 1:
				return new JPH::SphereShape(radius);
			case 2:
				return new JPH::CapsuleShape(std::max(component.halfHeight, 0.001f), radius);
			default: {
				const JPH::Vec3 halfExtents(std::max(component.halfExtents.x, 0.001f),
											std::max(component.halfExtents.y, 0.001f),
											std::max(component.halfExtents.z, 0.001f));
				return new JPH::BoxShape(halfExtents);
			}
			}
		}

	}

	struct PhysicsWorld::Impl : private JoltBoot, private LayerSetup {
		struct BodyEntry {
			JPH::BodyID id;
			uint32_t generation = 0;
			JPH::EMotionType motionType = JPH::EMotionType::Static;
		};

		explicit Impl(const Vec3& inOrigin)
			: objectVsBroadPhaseLayerFilter(broadPhaseLayerInterface, BroadPhaseLayers::Count,
											objectLayerPairFilter, Layers::Count) {
			physicsSystem.Init(1024, 0, 1024, 1024, broadPhaseLayerInterface,
							   objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
			physicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
			bodyInterface = &physicsSystem.GetBodyInterface();
			origin = inOrigin;
		}

		void syncBodies(EntityRegistry& registry) {
			reconcile(registry);

			if (bodiesAdded) {
				physicsSystem.OptimizeBroadPhase();
				bodiesAdded = false;
			}
		}

		void step(EntityRegistry& registry, float deltaTime) {
			syncBodies(registry);

			accumulator += std::clamp(deltaTime, 0.0f, kMaxFrameDelta);

			int steps = 0;
			while (accumulator >= kFixedStep && steps < kMaxSubSteps) {
				syncKinematic(registry);
				physicsSystem.Update(kFixedStep, 1, &tempAllocator, &jobSystem);
				accumulator -= kFixedStep;
				++steps;
			}

			if (steps > 0)
				writeBack(registry);

			if (steps == kMaxSubSteps)
				accumulator = 0.0f;
		}

		BodyEntry* find(uint32_t entityId) {
			auto it = bodies.find(entityId);
			return it == bodies.end() ? nullptr : &it->second;
		}

		const BodyEntry* find(uint32_t entityId) const {
			auto it = bodies.find(entityId);
			return it == bodies.end() ? nullptr : &it->second;
		}

		static JPH::RVec3 toPhysics(const Vec3& position, const Vec3& worldOrigin) {
			return JPH::RVec3(static_cast<float>(position.x - worldOrigin.x),
							  static_cast<float>(position.y - worldOrigin.y),
							  static_cast<float>(position.z - worldOrigin.z));
		}

		static JPH::Quat toPhysics(const glm::quat& rotation) {
			return JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w);
		}

		// EntityRegistry has no destroy hook, so bodies follow components by
		// reconciliation: no component means no body, and a reused slot (new
		// generation) rebuilds the body.
		void reconcile(EntityRegistry& registry) {
			for (uint32_t entityId : registry.view<RigidBodyComponent>()) {
				if (!registry.has<TransformComponent>(entityId))
					continue;

				const uint32_t generation = registry.getEntity(entityId).m_generation;
				auto it = bodies.find(entityId);
				if (it != bodies.end()) {
					if (it->second.generation == generation)
						continue;
					destroyBody(it->second);
					bodies.erase(it);
				}

				createBody(entityId, registry.get<RigidBodyComponent>(entityId),
						   registry.get<TransformComponent>(entityId), generation);
			}

			for (auto it = bodies.begin(); it != bodies.end(); ) {
				if (!registry.has<RigidBodyComponent>(it->first)) {
					destroyBody(it->second);
					it = bodies.erase(it);
				}
				else {
					++it;
				}
			}
		}

		void createBody(uint32_t entityId, const RigidBodyComponent& component,
						const TransformComponent& transform, uint32_t generation) {
			const int bodyType = std::clamp(component.bodyType, 0, 2);
			const JPH::EMotionType motionType = static_cast<JPH::EMotionType>(bodyType);
			const JPH::ObjectLayer layer =
				motionType == JPH::EMotionType::Static ? Layers::NonMoving : Layers::Moving;

			JPH::BodyCreationSettings settings(makeShape(component),
											   toPhysics(transform.position, origin),
											   toPhysics(transform.rotation), motionType, layer);
			settings.mFriction = std::max(component.friction, 0.0f);
			settings.mRestitution = std::max(component.restitution, 0.0f);
			settings.mLinearDamping = std::max(component.linearDamping, 0.0f);
			settings.mAngularDamping = std::max(component.angularDamping, 0.0f);
			settings.mGravityFactor = std::max(component.gravityFactor, 0.0f);

			if (motionType == JPH::EMotionType::Dynamic) {
				settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
				settings.mMassPropertiesOverride.mMass = std::max(component.mass, 0.001f);
			}

			if (component.lockRotation)
				settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX
									  | JPH::EAllowedDOFs::TranslationY
									  | JPH::EAllowedDOFs::TranslationZ;

			const JPH::BodyID id = bodyInterface->CreateAndAddBody(settings, JPH::EActivation::Activate);
			bodies[entityId] = BodyEntry{ id, generation, motionType };
			bodiesAdded = true;
		}

		void destroyBody(const BodyEntry& entry) {
			if (entry.id.IsInvalid())
				return;
			bodyInterface->RemoveBody(entry.id);
			bodyInterface->DestroyBody(entry.id);
		}

		void syncKinematic(EntityRegistry& registry) {
			for (auto& [entityId, entry] : bodies) {
				if (entry.motionType != JPH::EMotionType::Kinematic)
					continue;
				if (!registry.has<TransformComponent>(entityId))
					continue;

				const TransformComponent& transform = registry.get<TransformComponent>(entityId);
				bodyInterface->MoveKinematic(entry.id, toPhysics(transform.position, origin),
											 toPhysics(transform.rotation), kFixedStep);
			}
		}

		void writeBack(EntityRegistry& registry) {
			for (auto& [entityId, entry] : bodies) {
				if (entry.motionType != JPH::EMotionType::Dynamic)
					continue;
				if (!registry.has<TransformComponent>(entityId))
					continue;

				JPH::RVec3 position;
				JPH::Quat rotation;
				bodyInterface->GetPositionAndRotation(entry.id, position, rotation);

				TransformComponent& transform = registry.get<TransformComponent>(entityId);
				transform.position = Vec3(origin.x + static_cast<double>(position.GetX()),
										 origin.y + static_cast<double>(position.GetY()),
										 origin.z + static_cast<double>(position.GetZ()));
				transform.rotation = glm::quat(rotation.GetW(), rotation.GetX(),
											   rotation.GetY(), rotation.GetZ());
			}
		}

		JPH::ObjectVsBroadPhaseLayerFilterTable objectVsBroadPhaseLayerFilter;
		JPH::PhysicsSystem physicsSystem;
		JPH::TempAllocatorImpl tempAllocator{ 16 * 1024 * 1024 };
		JPH::JobSystemThreadPool jobSystem{ 2048, 8, 2 };
		JPH::BodyInterface* bodyInterface = nullptr;

		Vec3 origin{ 0.0 };
		std::unordered_map<uint32_t, BodyEntry> bodies;
		float accumulator = 0.0f;
		bool bodiesAdded = false;
	};

	PhysicsWorld::PhysicsWorld(const Vec3& inOrigin)
		: m_impl(CreateScope<Impl>(inOrigin)) {
	}

	PhysicsWorld::~PhysicsWorld() = default;

	void PhysicsWorld::step(EntityRegistry& registry, float deltaTime) {
		m_impl->step(registry, deltaTime);
	}

	void PhysicsWorld::syncBodies(EntityRegistry& registry) {
		m_impl->syncBodies(registry);
	}

	void PhysicsWorld::setGravity(const glm::vec3& gravity) {
		m_impl->physicsSystem.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
	}

	void PhysicsWorld::applyImpulse(uint32_t entityId, const glm::vec3& impulse) {
		Impl::BodyEntry* entry = m_impl->find(entityId);
		if (!entry || entry->motionType != JPH::EMotionType::Dynamic)
			return;

		m_impl->bodyInterface->ActivateBody(entry->id);
		m_impl->bodyInterface->AddImpulse(entry->id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
	}

	void PhysicsWorld::setLinearVelocity(uint32_t entityId, const glm::vec3& velocity) {
		Impl::BodyEntry* entry = m_impl->find(entityId);
		if (!entry || entry->motionType != JPH::EMotionType::Dynamic)
			return;

		m_impl->bodyInterface->SetLinearVelocity(entry->id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
		m_impl->bodyInterface->ActivateBody(entry->id);
	}

	glm::vec3 PhysicsWorld::getLinearVelocity(uint32_t entityId) const {
		const Impl::BodyEntry* entry = m_impl->find(entityId);
		if (!entry)
			return glm::vec3(0.0f);

		const JPH::Vec3 velocity = m_impl->bodyInterface->GetLinearVelocity(entry->id);
		return glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
	}

	void PhysicsWorld::teleport(uint32_t entityId, const Vec3& position, const glm::quat& rotation) {
		Impl::BodyEntry* entry = m_impl->find(entityId);
		if (!entry)
			return;

		m_impl->bodyInterface->SetPositionAndRotation(
			entry->id, Impl::toPhysics(position, m_impl->origin),
			Impl::toPhysics(rotation), JPH::EActivation::Activate);
	}

	bool PhysicsWorld::hasBody(uint32_t entityId) const {
		return m_impl->bodies.find(entityId) != m_impl->bodies.end();
	}

}

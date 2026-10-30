#include "vepch.h"
#include "Modules.h"

#include "EmitterAsset.h"
#include "ProgramBuilder.h"

namespace ve::builtin {

	namespace {

		void gravityEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg accel = builder.param(entry, "Accel");
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg velocity = builder.loadAttr("Velocity");
			const Reg newVelocity = builder.add(velocity, builder.mul(accel, dt));
			builder.storeAttr("Velocity", newVelocity);

			const Reg position = builder.loadAttr("Position");
			const Reg newPosition = builder.add(position, builder.mul(newVelocity, dt));
			builder.storeAttr("Position", newPosition);
		}

		// Gravity with an arbitrary direction: the parameter carries both the axis and
		// the magnitude of a constant acceleration.
		void linearForceEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg force = builder.param(entry, "Force");
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg velocity = builder.add(builder.loadAttr("Velocity"), builder.mul(force, dt));
			builder.storeAttr("Velocity", velocity);

			const Reg position = builder.loadAttr("Position");
			builder.storeAttr("Position", builder.add(position, builder.mul(velocity, dt)));
		}

		// Radial push away from Center. Normalizing the offset keeps Strength a pure
		// acceleration: without it the force would grow with distance from the center.
		void pointForceEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg strength = builder.param(entry, "Strength");
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg position = builder.loadAttr("Position");
			const Reg direction = builder.normalize(builder.sub(position, center));
			const Reg accel = builder.mul(direction, strength);
			const Reg velocity = builder.add(builder.loadAttr("Velocity"), builder.mul(accel, dt));
			builder.storeAttr("Velocity", velocity);
			builder.storeAttr("Position", builder.add(position, builder.mul(velocity, dt)));
		}

		// SpawnRate reads SpawnCarry before writing it, so the attribute has to exist in the
		// emitter layout before EmitterUpdate compiles; this module declares it once per activation.
		void initEmitterEmit(ProgramBuilder& builder, const ModuleEntry&) {
			builder.storeAttr("SpawnCarry", builder.imm(0.0f));
		}

		void initParticleEmit(ProgramBuilder& builder, const ModuleEntry&) {
			const float position[3] = { 0.0f, 0.0f, 0.0f };
			const float velocity[3] = { 0.0f, 0.0f, 100.0f };

			builder.storeAttr("Position", builder.imm(position, 3));
			builder.storeAttr("Velocity", builder.imm(velocity, 3));
			builder.storeAttr("Age", builder.imm(0.0f));
			builder.storeAttr("Lifetime", builder.imm(2.0f));
		}

		// Uniform inside a ball. Must run after InitParticle in ParticleSpawn: later
		// writes to Position win, so this overwrites the constant spawn position.
		void shapeLocationEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg radius = builder.param(entry, "Radius");
			const Reg seed = builder.implicit(Implicit::SpawnRandom);

			// Uniform direction: z uniform on [-1,1], azimuth uniform on [0,2pi).
			const Reg z = builder.sub(builder.imm(1.0f), builder.mul(builder.random(seed, 0), builder.imm(2.0f)));
			const Reg phi = builder.mul(builder.random(seed, 1), builder.imm(6.2831853f));
			const Reg ring = builder.sqrt(builder.sub(builder.imm(1.0f), builder.mul(z, z)));
			const Reg direction = builder.compose(
				builder.mul(ring, builder.cos(phi)),
				builder.mul(ring, builder.sin(phi)),
				z);

			// max() of three uniforms has CDF t^3, the radial density of a uniform ball.
			const Reg u = builder.max(builder.max(builder.random(seed, 2), builder.random(seed, 3)),
				builder.random(seed, 4));
			const Reg distance = builder.mul(radius, u);

			builder.storeAttr("Position", builder.add(center, builder.mul(direction, distance)));
		}

		void spawnRateEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg rate = builder.param(entry, "Rate");
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg carry = builder.loadAttr("SpawnCarry");
			const Reg total = builder.add(carry, builder.mul(rate, dt));
			const Reg count = builder.floor(total);

			builder.storeAttr("SpawnCarry", builder.sub(total, count));
			builder.storeAttr("SpawnInfo0.Count", count);
			builder.storeAttr("SpawnInfo0.InterpStartDt", builder.imm(0.0f));
			builder.storeAttr("SpawnInfo0.IntervalDt", builder.div(dt, builder.max(count, builder.imm(1.0f))));
			builder.storeAttr("SpawnInfo0.SpawnGroup", builder.imm(0.0f));
		}

		void lifetimeEmit(ProgramBuilder& builder, const ModuleEntry&) {
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg age = builder.add(builder.loadAttr("Age"), dt);

			builder.storeAttr("Age", age);
			builder.setAlive(builder.less(age, builder.loadAttr("Lifetime")));
		}

		const ModuleAsset kGravity = {
			"Gravity", 1, stageBit(Stage::ParticleUpdate),
			{ { "Accel", { 0.0f, 0.0f, -980.0f, 0.0f }, 3 } },
			&gravityEmit
		};

		const ModuleAsset kLinearForce = {
			"LinearForce", 1, stageBit(Stage::ParticleUpdate),
			{ { "Force", { 0.0f, 0.0f, 500.0f, 0.0f }, 3 } },
			&linearForceEmit
		};

		const ModuleAsset kPointForce = {
			"PointForce", 1, stageBit(Stage::ParticleUpdate),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "Strength", { 500.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&pointForceEmit
		};

		const ModuleAsset kInitEmitter = {
			"InitEmitter", 1, stageBit(Stage::EmitterSpawn), {}, &initEmitterEmit
		};

		const ModuleAsset kInitParticle = {
			"InitParticle", 1, stageBit(Stage::ParticleSpawn), {}, &initParticleEmit
		};

		const ModuleAsset kShapeLocation = {
			"ShapeLocation", 1, stageBit(Stage::ParticleSpawn),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "Radius", { 200.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&shapeLocationEmit
		};

		const ModuleAsset kSpawnRate = {
			"SpawnRate", 1, stageBit(Stage::EmitterUpdate),
			{ { "Rate", { 100.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&spawnRateEmit
		};

		const ModuleAsset kLifetime = {
			"Lifetime", 1, stageBit(Stage::ParticleUpdate), {}, &lifetimeEmit
		};

	}

	const ModuleAsset& gravity() { return kGravity; }
	const ModuleAsset& linearForce() { return kLinearForce; }
	const ModuleAsset& pointForce() { return kPointForce; }
	const ModuleAsset& initEmitter() { return kInitEmitter; }
	const ModuleAsset& initParticle() { return kInitParticle; }
	const ModuleAsset& shapeLocation() { return kShapeLocation; }
	const ModuleAsset& spawnRate() { return kSpawnRate; }
	const ModuleAsset& lifetime() { return kLifetime; }

	const std::vector<const ModuleAsset*>& builtinModules() {
		static const std::vector<const ModuleAsset*> modules = {
			&kInitEmitter, &kSpawnRate, &kInitParticle, &kShapeLocation,
			&kGravity, &kLinearForce, &kPointForce, &kLifetime
		};
		return modules;
	}

}

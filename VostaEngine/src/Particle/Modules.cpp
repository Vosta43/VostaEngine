#include "vepch.h"
#include "Modules.h"

#include "EmitterAsset.h"
#include "ProgramBuilder.h"

namespace ve::builtin {

	namespace {

		constexpr float kDegToRad = 0.017453293f;
		constexpr float kTwoPi = 6.2831853f;

		// The VM has no min/abs/dot/cross opcodes; these compose the existing ones.
		Reg absOf(ProgramBuilder& builder, const Reg& value) {
			return builder.max(value, builder.mul(value, builder.imm(-1.0f)));
		}

		Reg dot(ProgramBuilder& builder, const Reg& a, const Reg& b) {
			return builder.add(
				builder.add(builder.mul(builder.extract(a, 0), builder.extract(b, 0)),
					builder.mul(builder.extract(a, 1), builder.extract(b, 1))),
				builder.mul(builder.extract(a, 2), builder.extract(b, 2)));
		}

		Reg cross(ProgramBuilder& builder, const Reg& a, const Reg& b) {
			const Reg ax = builder.extract(a, 0), ay = builder.extract(a, 1), az = builder.extract(a, 2);
			const Reg bx = builder.extract(b, 0), by = builder.extract(b, 1), bz = builder.extract(b, 2);
			return builder.compose(
				builder.sub(builder.mul(ay, bz), builder.mul(az, by)),
				builder.sub(builder.mul(az, bx), builder.mul(ax, bz)),
				builder.sub(builder.mul(ax, by), builder.mul(ay, bx)));
		}

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

		// SpawnRate and SpawnBurst read their carry attributes before writing them, so the
		// attributes have to exist in the emitter layout before EmitterUpdate compiles;
		// this module declares them once per activation.
		void initEmitterEmit(ProgramBuilder& builder, const ModuleEntry&) {
			builder.storeAttr("SpawnCarry", builder.imm(0.0f));
			builder.storeAttr("BurstCarry", builder.imm(0.0f));
			builder.storeAttr("BurstFired", builder.imm(0.0f));
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

		// Box, disc, cone and ring spawns all overwrite Position, so they behave like
		// ShapeLocation: place them after InitParticle, and use only one of them.
		void shapeBoxEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg halfExtent = builder.param(entry, "HalfExtent");
			const Reg seed = builder.implicit(Implicit::SpawnRandom);

			const Reg offset = builder.compose(
				builder.mul(builder.sub(builder.mul(builder.random(seed, 0), builder.imm(2.0f)), builder.imm(1.0f)),
					builder.extract(halfExtent, 0)),
				builder.mul(builder.sub(builder.mul(builder.random(seed, 1), builder.imm(2.0f)), builder.imm(1.0f)),
					builder.extract(halfExtent, 1)),
				builder.mul(builder.sub(builder.mul(builder.random(seed, 2), builder.imm(2.0f)), builder.imm(1.0f)),
					builder.extract(halfExtent, 2)));

			builder.storeAttr("Position", builder.add(center, offset));
		}

		// Flat disc in the XY plane, the plane the editor's Z-up preview shows.
		void shapeDiscEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg radius = builder.param(entry, "Radius");
			const Reg seed = builder.implicit(Implicit::SpawnRandom);

			// sqrt() makes the sample area-uniform instead of center-heavy.
			const Reg r = builder.mul(radius, builder.sqrt(builder.random(seed, 3)));
			const Reg phi = builder.mul(builder.random(seed, 4), builder.imm(kTwoPi));
			const Reg offset = builder.compose(builder.mul(builder.cos(phi), r),
				builder.mul(builder.sin(phi), r), builder.imm(0.0f));

			builder.storeAttr("Position", builder.add(center, offset));
		}

		void shapeConeEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg radius = builder.param(entry, "Radius");
			const Reg height = builder.param(entry, "Height");
			const Reg seed = builder.implicit(Implicit::SpawnRandom);

			const Reg u = builder.random(seed, 5);
			const Reg z = builder.mul(u, height);
			const Reg r = builder.mul(builder.mul(radius, builder.sub(builder.imm(1.0f), u)),
				builder.sqrt(builder.random(seed, 6)));
			const Reg phi = builder.mul(builder.random(seed, 7), builder.imm(kTwoPi));
			const Reg offset = builder.compose(builder.mul(builder.cos(phi), r),
				builder.mul(builder.sin(phi), r), z);

			builder.storeAttr("Position", builder.add(center, offset));
		}

		// Thin annulus in the XY plane; Thickness spreads the radius on both sides.
		void shapeRingEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg radius = builder.param(entry, "Radius");
			const Reg thickness = builder.param(entry, "Thickness");
			const Reg seed = builder.implicit(Implicit::SpawnRandom);

			const Reg spread = builder.mul(builder.sub(builder.mul(builder.random(seed, 8), builder.imm(2.0f)),
				builder.imm(1.0f)), thickness);
			const Reg r = builder.add(radius, spread);
			const Reg phi = builder.mul(builder.random(seed, 9), builder.imm(kTwoPi));
			const Reg offset = builder.compose(builder.mul(builder.cos(phi), r),
				builder.mul(builder.sin(phi), r), builder.imm(0.0f));

			builder.storeAttr("Position", builder.add(center, offset));
		}

		// Speed drawn uniformly in [SpeedMin, SpeedMax], direction drawn inside a cone
		// around Direction. Overwrites InitParticle's constant velocity.
		void initialVelocityEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg direction = builder.normalize(builder.param(entry, "Direction"));
			const Reg minSpeed = builder.param(entry, "SpeedMin");
			const Reg maxSpeed = builder.param(entry, "SpeedMax");
			const Reg halfAngle = builder.mul(builder.param(entry, "ConeAngle"), builder.imm(kDegToRad));
			const Reg seed = builder.implicit(Implicit::SpawnRandom);

			// Tangent frame around Direction. The helper axis must not be parallel to
			// it, so use +Z unless Direction is already near vertical.
			const Reg horizontal = builder.less(absOf(builder, builder.extract(direction, 2)), builder.imm(0.999f));
			const Reg helper = builder.compose(builder.sub(builder.imm(1.0f), horizontal),
				builder.imm(0.0f), horizontal);
			const Reg tangent = builder.normalize(cross(builder, direction, helper));
			const Reg bitangent = cross(builder, direction, tangent);

			// cos(theta) uniform in [cos(halfAngle), 1] gives a uniform solid angle.
			const Reg cosMax = builder.cos(halfAngle);
			const Reg cosTheta = builder.sub(builder.imm(1.0f),
				builder.mul(builder.random(seed, 10), builder.sub(builder.imm(1.0f), cosMax)));
			const Reg sinTheta = builder.sqrt(builder.max(
				builder.sub(builder.imm(1.0f), builder.mul(cosTheta, cosTheta)), builder.imm(0.0f)));
			const Reg phi = builder.mul(builder.random(seed, 11), builder.imm(kTwoPi));
			const Reg spread = builder.add(
				builder.mul(tangent, builder.mul(builder.cos(phi), sinTheta)),
				builder.mul(bitangent, builder.mul(builder.sin(phi), sinTheta)));
			const Reg localDirection = builder.add(builder.mul(direction, cosTheta), spread);

			const Reg speed = builder.add(minSpeed,
				builder.mul(builder.sub(maxSpeed, minSpeed), builder.random(seed, 12)));
			builder.storeAttr("Velocity", builder.mul(localDirection, speed));
		}

		void lifetimeRangeEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg minLife = builder.param(entry, "Min");
			const Reg maxLife = builder.param(entry, "Max");
			const Reg t = builder.random(builder.implicit(Implicit::SpawnRandom), 13);

			builder.storeAttr("Lifetime", builder.add(minLife, builder.mul(builder.sub(maxLife, minLife), t)));
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

		// Count > 0 with Interval <= 0 emits one burst on the first update tick.
		// Interval > 0 repeats the burst every Interval seconds. Uses slot 1 so a
		// SpawnRate can run in the same emitter without overwriting the burst.
		void spawnBurstEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg count = builder.param(entry, "Count");
			const Reg interval = builder.param(entry, "Interval");
			const Reg dt = builder.implicit(Implicit::Dt);

			const Reg carry = builder.loadAttr("BurstCarry");
			const Reg total = builder.add(carry, dt);
			const Reg repeated = builder.less(interval, total);
			const Reg repeating = builder.less(builder.imm(0.0f), interval);
			const Reg firedOnce = builder.less(builder.loadAttr("BurstFired"), builder.imm(0.5f));

			// fired = repeating ? repeated : firedOnce
			const Reg oneShot = builder.mul(builder.sub(builder.imm(1.0f), repeating), firedOnce);
			const Reg fired = builder.add(builder.mul(repeating, repeated), oneShot);

			builder.storeAttr("BurstFired", builder.imm(1.0f));
			builder.storeAttr("BurstCarry", builder.sub(total, builder.mul(interval, repeated)));
			builder.storeAttr("SpawnInfo1.Count", builder.mul(count, fired));
			builder.storeAttr("SpawnInfo1.InterpStartDt", builder.imm(0.0f));
			builder.storeAttr("SpawnInfo1.IntervalDt", builder.imm(0.0f));
			builder.storeAttr("SpawnInfo1.SpawnGroup", builder.imm(0.0f));
		}

		void initColorEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			builder.storeAttr("Color", builder.param(entry, "Color"));
		}

		void initEmissiveEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			builder.storeAttr("Emissive", builder.param(entry, "Emissive"));
		}

		void initSizeRangeEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg minSize = builder.param(entry, "Min");
			const Reg maxSize = builder.param(entry, "Max");
			const Reg t = builder.random(builder.implicit(Implicit::SpawnRandom), 14);

			builder.storeAttr("Size", builder.add(minSize, builder.mul(builder.sub(maxSize, minSize), t)));
		}

		// Every velocity module integrates Position, matching Gravity and the other
		// forces: a particle needs at least one of them in ParticleUpdate to move.
		void dragEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg drag = builder.param(entry, "Drag");
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg factor = builder.max(builder.sub(builder.imm(1.0f), builder.mul(drag, dt)), builder.imm(0.0f));

			const Reg velocity = builder.mul(builder.loadAttr("Velocity"), factor);
			builder.storeAttr("Velocity", velocity);
			builder.storeAttr("Position", builder.add(builder.loadAttr("Position"), builder.mul(velocity, dt)));
		}

		void speedLimitEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg maxSpeed = builder.param(entry, "MaxSpeed");
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg velocity = builder.loadAttr("Velocity");
			const Reg speed = builder.length(velocity);

			// Scale down only when over the limit; length() is 0 at rest, so divide by
			// a floored magnitude rather than risk a division by zero.
			const Reg ratio = builder.div(maxSpeed, builder.max(speed, builder.imm(1e-6f)));
			const Reg over = builder.less(maxSpeed, speed);
			const Reg factor = builder.add(builder.imm(1.0f),
				builder.mul(builder.sub(ratio, builder.imm(1.0f)), over));

			const Reg limited = builder.mul(velocity, factor);
			builder.storeAttr("Velocity", limited);
			builder.storeAttr("Position", builder.add(builder.loadAttr("Position"), builder.mul(limited, dt)));
		}

		// Swirls particles around Axis through Center. Only the radial (in-plane)
		// part gets the tangential push, so particles do not drift along the axis.
		void vortexEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg axis = builder.normalize(builder.param(entry, "Axis"));
			const Reg strength = builder.param(entry, "Strength");
			const Reg dt = builder.implicit(Implicit::Dt);

			const Reg position = builder.loadAttr("Position");
			const Reg radial = builder.sub(position, center);
			const Reg inPlane = builder.sub(radial, builder.mul(axis, dot(builder, radial, axis)));
			const Reg tangent = builder.normalize(cross(builder, axis, inPlane));

			const Reg velocity = builder.add(builder.loadAttr("Velocity"),
				builder.mul(builder.mul(tangent, strength), dt));
			builder.storeAttr("Velocity", velocity);
			builder.storeAttr("Position", builder.add(position, builder.mul(velocity, dt)));
		}

		// Kill modules multiply into Alive instead of overwriting it, so an earlier
		// Lifetime expiry is not resurrected and several kill volumes combine.
		void killSphereEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg radius = builder.param(entry, "Radius");
			const Reg distance = builder.length(builder.sub(builder.loadAttr("Position"), center));

			builder.setAlive(builder.mul(builder.implicit(Implicit::Alive), builder.less(distance, radius)));
		}

		void killBoxEmit(ProgramBuilder& builder, const ModuleEntry& entry) {
			const Reg center = builder.param(entry, "Center");
			const Reg halfExtent = builder.param(entry, "HalfExtent");
			const Reg local = absOf(builder, builder.sub(builder.loadAttr("Position"), center));
			const Reg inside = builder.less(builder.sub(local, halfExtent), builder.imm(0.0f));

			const Reg keep = builder.mul(builder.mul(builder.extract(inside, 0), builder.extract(inside, 1)),
				builder.extract(inside, 2));
			builder.setAlive(builder.mul(builder.implicit(Implicit::Alive), keep));
		}

		void lifetimeEmit(ProgramBuilder& builder, const ModuleEntry&) {
			const Reg dt = builder.implicit(Implicit::Dt);
			const Reg age = builder.add(builder.loadAttr("Age"), dt);

			builder.storeAttr("Age", age);
			// Composes with Alive for the same reason the kill modules do: any module
			// may be the one that kills a particle, and none may revive it.
			builder.setAlive(builder.mul(builder.implicit(Implicit::Alive),
				builder.less(age, builder.loadAttr("Lifetime"))));
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

		const ModuleAsset kShapeBox = {
			"ShapeBox", 1, stageBit(Stage::ParticleSpawn),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "HalfExtent", { 100.0f, 100.0f, 100.0f, 0.0f }, 3 } },
			&shapeBoxEmit
		};

		const ModuleAsset kShapeDisc = {
			"ShapeDisc", 1, stageBit(Stage::ParticleSpawn),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "Radius", { 200.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&shapeDiscEmit
		};

		const ModuleAsset kShapeCone = {
			"ShapeCone", 1, stageBit(Stage::ParticleSpawn),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "Radius", { 200.0f, 0.0f, 0.0f, 0.0f }, 1 },
			  { "Height", { 200.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&shapeConeEmit
		};

		const ModuleAsset kShapeRing = {
			"ShapeRing", 1, stageBit(Stage::ParticleSpawn),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "Radius", { 200.0f, 0.0f, 0.0f, 0.0f }, 1 },
			  { "Thickness", { 20.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&shapeRingEmit
		};

		const ModuleAsset kInitialVelocity = {
			"InitialVelocity", 1, stageBit(Stage::ParticleSpawn),
			{ { "Direction", { 0.0f, 0.0f, 1.0f, 0.0f }, 3 },
			  { "SpeedMin", { 100.0f, 0.0f, 0.0f, 0.0f }, 1 },
			  { "SpeedMax", { 200.0f, 0.0f, 0.0f, 0.0f }, 1 },
			  { "ConeAngle", { 15.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&initialVelocityEmit
		};

		const ModuleAsset kLifetimeRange = {
			"LifetimeRange", 1, stageBit(Stage::ParticleSpawn),
			{ { "Min", { 1.0f, 0.0f, 0.0f, 0.0f }, 1 },
			  { "Max", { 2.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&lifetimeRangeEmit
		};

		const ModuleAsset kInitColor = {
			"InitColor", 1, stageBit(Stage::ParticleSpawn),
			{ { "Color", { 1.0f, 1.0f, 1.0f, 1.0f }, 4 } },
			&initColorEmit
		};

		const ModuleAsset kInitEmissive = {
			"InitEmissive", 1, stageBit(Stage::ParticleSpawn),
			{ { "Emissive", { 0.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&initEmissiveEmit
		};

		const ModuleAsset kInitSizeRange = {
			"InitSizeRange", 1, stageBit(Stage::ParticleSpawn),
			{ { "Min", { 10.0f, 0.0f, 0.0f, 0.0f }, 1 },
			  { "Max", { 20.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&initSizeRangeEmit
		};

		const ModuleAsset kSpawnRate = {
			"SpawnRate", 1, stageBit(Stage::EmitterUpdate),
			{ { "Rate", { 100.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&spawnRateEmit
		};

		const ModuleAsset kSpawnBurst = {
			"SpawnBurst", 1, stageBit(Stage::EmitterUpdate),
			{ { "Count", { 10.0f, 0.0f, 0.0f, 0.0f }, 1 },
			  { "Interval", { 0.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&spawnBurstEmit
		};

		const ModuleAsset kLifetime = {
			"Lifetime", 1, stageBit(Stage::ParticleUpdate), {}, &lifetimeEmit
		};

		const ModuleAsset kDrag = {
			"Drag", 1, stageBit(Stage::ParticleUpdate),
			{ { "Drag", { 1.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&dragEmit
		};

		const ModuleAsset kSpeedLimit = {
			"SpeedLimit", 1, stageBit(Stage::ParticleUpdate),
			{ { "MaxSpeed", { 500.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&speedLimitEmit
		};

		const ModuleAsset kVortex = {
			"Vortex", 1, stageBit(Stage::ParticleUpdate),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "Axis", { 0.0f, 0.0f, 1.0f, 0.0f }, 3 },
			  { "Strength", { 300.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&vortexEmit
		};

		const ModuleAsset kKillSphere = {
			"KillSphere", 1, stageBit(Stage::ParticleUpdate),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "Radius", { 3000.0f, 0.0f, 0.0f, 0.0f }, 1 } },
			&killSphereEmit
		};

		const ModuleAsset kKillBox = {
			"KillBox", 1, stageBit(Stage::ParticleUpdate),
			{ { "Center", { 0.0f, 0.0f, 0.0f, 0.0f }, 3 },
			  { "HalfExtent", { 2000.0f, 2000.0f, 2000.0f, 0.0f }, 3 } },
			&killBoxEmit
		};

	}

	const ModuleAsset& gravity() { return kGravity; }
	const ModuleAsset& linearForce() { return kLinearForce; }
	const ModuleAsset& pointForce() { return kPointForce; }
	const ModuleAsset& initEmitter() { return kInitEmitter; }
	const ModuleAsset& initParticle() { return kInitParticle; }
	const ModuleAsset& shapeLocation() { return kShapeLocation; }
	const ModuleAsset& shapeBox() { return kShapeBox; }
	const ModuleAsset& shapeDisc() { return kShapeDisc; }
	const ModuleAsset& shapeCone() { return kShapeCone; }
	const ModuleAsset& shapeRing() { return kShapeRing; }
	const ModuleAsset& initialVelocity() { return kInitialVelocity; }
	const ModuleAsset& lifetimeRange() { return kLifetimeRange; }
	const ModuleAsset& initColor() { return kInitColor; }
	const ModuleAsset& initEmissive() { return kInitEmissive; }
	const ModuleAsset& initSizeRange() { return kInitSizeRange; }
	const ModuleAsset& spawnRate() { return kSpawnRate; }
	const ModuleAsset& spawnBurst() { return kSpawnBurst; }
	const ModuleAsset& lifetime() { return kLifetime; }
	const ModuleAsset& drag() { return kDrag; }
	const ModuleAsset& speedLimit() { return kSpeedLimit; }
	const ModuleAsset& vortex() { return kVortex; }
	const ModuleAsset& killSphere() { return kKillSphere; }
	const ModuleAsset& killBox() { return kKillBox; }

	const std::vector<const ModuleAsset*>& builtinModules() {
		static const std::vector<const ModuleAsset*> modules = {
			&kInitEmitter, &kSpawnRate, &kSpawnBurst, &kInitParticle,
			&kShapeLocation, &kShapeBox, &kShapeDisc, &kShapeCone, &kShapeRing,
			&kInitialVelocity, &kLifetimeRange, &kInitColor, &kInitEmissive, &kInitSizeRange,
			&kGravity, &kLinearForce, &kPointForce, &kLifetime,
			&kDrag, &kSpeedLimit, &kVortex, &kKillSphere, &kKillBox
		};
		return modules;
	}

}

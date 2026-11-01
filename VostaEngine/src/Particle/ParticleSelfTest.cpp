#include "vepch.h"
#include "ParticleSelfTest.h"

#include "AttributeRegistry.h"
#include "EmitterAsset.h"
#include "EmitterCompiler.h"
#include "Modules.h"
#include "ParticleDataSet.h"
#include "ParticleEmitter.h"
#include "ParticleSystem.h"
#include "Program.h"
#include "ProgramBuilder.h"
#include "VM.h"
#include "Core/Log.h"

#include <cmath>

namespace ve {

	namespace {

		bool check(bool condition, const char* what) {
			if (!condition)
				VE_CORE_ERROR_PRINT("ParticleSelfTest: %s", what);
			return condition;
		}

		bool testRegistry() {
			AttributeRegistry registry;
			bool ok = true;

			ok &= check(registry.getOrAdd("Position", 3) == 0, "Position offset");
			ok &= check(registry.getOrAdd("Velocity", 3) == 3, "Velocity offset");
			ok &= check(registry.getOrAdd("Age", 1) == 6, "Age offset");
			ok &= check(registry.getOrAdd("Position", 3) == 0, "duplicate keeps offset");
			ok &= check(registry.totalComponentsCount() == 7, "total components");
			ok &= check(registry.slotCount() == 3, "slot count");

			const AttributeDesc* velocity = registry.find("Velocity");
			ok &= check(velocity && velocity->offset == 3 && velocity->comps == 3, "find width and offset");
			ok &= check(registry.find("Missing") == nullptr, "missing attribute");

			return ok;
		}

		bool testDataSet() {
			ParticleDataSet set;
			set.stride = 3;
			set.ensureCapacity(1500);

			bool ok = true;
			ok &= check(set.capacity >= 1500, "capacity growth");
			ok &= check(set.data.size() == static_cast<size_t>(set.capacity) * set.stride, "data size");

			set.num = 2;
			for (uint16_t c = 0; c < 3; ++c) {
				*set.attr(c, 0) = static_cast<float>(c + 1);
				*set.attr(c, 1) = static_cast<float>(c + 11);
			}

			set.zeroInstance(1);
			ok &= check(*set.attr(0, 1) == 0.0f && *set.attr(1, 1) == 0.0f && *set.attr(2, 1) == 0.0f,
				"zeroInstance clears the whole stride");
			ok &= check(*set.attr(2, 0) == 3.0f, "zeroInstance leaves other instances alone");

			for (uint16_t c = 0; c < 3; ++c)
				*set.attr(c, 1) = static_cast<float>(c + 11);

			ParticleDataSet dst;
			dst.stride = 3;
			dst.ensureCapacity(1);
			dst.copyInstance(set, 1, 0);
			ok &= check(*dst.attr(0, 0) == 11.0f && *dst.attr(1, 0) == 12.0f && *dst.attr(2, 0) == 13.0f,
				"copyInstance across buffers");

			set.ensureCapacity(4000);
			ok &= check(set.capacity >= 4000, "second growth");
			ok &= check(*set.attr(0, 0) == 1.0f && *set.attr(1, 1) == 12.0f, "growth preserves instances");

			return ok;
		}

		bool testVM() {
			Program program;
			program.attrs.push_back({ 0, 0, 3 });
			program.attrs.push_back({ 1, 3, 3 });
			program.code.push_back({ Op::LoadAttr, 3, 7, 0, 0, 0, 0 });
			program.code.push_back({ Op::LoadAttr, 3, 10, 0, 0, 0, 1 });
			program.code.push_back({ Op::Mul, 3, 13, 10, 1, kFlagScalarB, 0 });
			program.code.push_back({ Op::Add, 3, 16, 7, 13, 0, 0 });
			program.code.push_back({ Op::StoreAttr, 3, 0, 16, 0, 0, 0 });
			program.regCount = 19;

			ParticleDataSet set;
			set.stride = 6;
			set.ensureCapacity(4);
			set.num = 1;
			*set.attr(0, 0) = 1.0f;
			*set.attr(1, 0) = 2.0f;
			*set.attr(2, 0) = 3.0f;
			*set.attr(3, 0) = 2.0f;
			*set.attr(4, 0) = 4.0f;
			*set.attr(5, 0) = 6.0f;

			ExecContext context;
			context.program = &program;
			context.src = &set;
			context.dst = &set;
			context.dt = 0.5f;

			bool ok = true;
			ok &= check(VM::run(context), "particle stays alive");
			ok &= check(*set.attr(0, 0) == 2.0f && *set.attr(1, 0) == 4.0f && *set.attr(2, 0) == 6.0f,
				"position += velocity * dt");

			Program normalize;
			normalize.attrs.push_back({ 0, 0, 2 });
			normalize.consts.push_back(3.0f);
			normalize.consts.push_back(4.0f);
			normalize.code.push_back({ Op::LoadImm, 1, 7, 0, 0, 0, 0 });
			normalize.code.push_back({ Op::LoadImm, 1, 8, 0, 0, 0, 1 });
			normalize.code.push_back({ Op::Normalize, 2, 9, 7, 0, 0, 0 });
			normalize.code.push_back({ Op::StoreAttr, 2, 0, 9, 0, 0, 0 });
			normalize.regCount = 11;

			ExecContext normalizeContext;
			normalizeContext.program = &normalize;
			normalizeContext.src = &set;
			normalizeContext.dst = &set;
			ok &= check(VM::run(normalizeContext), "normalize keeps the particle alive");
			ok &= check(std::fabs(*set.attr(0, 0) - 0.6f) < 1e-5f &&
				std::fabs(*set.attr(1, 0) - 0.8f) < 1e-5f,
				"Normalize turns (3,4) into (0.6,0.8)");

			Program dead;
			dead.consts.push_back(0.0f);
			dead.code.push_back({ Op::LoadImm, 1, 7, 0, 0, 0, 0 });
			dead.code.push_back({ Op::SetAlive, 1, static_cast<uint8_t>(kAliveReg), 7, 0, 0, 0 });

			ExecContext deadContext;
			deadContext.program = &dead;
			deadContext.src = &set;
			deadContext.dst = &set;
			ok &= check(!VM::run(deadContext), "SetAlive 0 kills the particle");

			return ok;
		}

		bool testModules() {
			AttributeRegistry layout;
			layout.getOrAdd("Position", 3);
			layout.getOrAdd("Velocity", 3);
			layout.getOrAdd("Age", 1);
			layout.getOrAdd("Lifetime", 1);
			layout.getOrAdd("SpawnCarry", 1);
			layout.getOrAdd("SpawnInfo0.Count", 1);
			layout.getOrAdd("SpawnInfo0.InterpStartDt", 1);
			layout.getOrAdd("SpawnInfo0.IntervalDt", 1);
			layout.getOrAdd("SpawnInfo0.SpawnGroup", 1);

			bool ok = true;

			{
				ModuleEntry entry{ &builtin::initParticle(), { 1, 1 }, {} };
				ProgramBuilder builder(&layout);
				entry.module->emit(builder, entry);
				ok &= check(builder.valid(), "InitParticle emits");
				ok &= check(builder.code.size() == 12, "InitParticle instruction count");
			}

			{
				ModuleEntry entry{ &builtin::gravity(), { 2, 2 }, {} };
				ProgramBuilder builder(&layout);
				entry.module->emit(builder, entry);
				ok &= check(builder.valid(), "Gravity emits");
				ok &= check(builder.code.size() == 9, "Gravity instruction count");
				ok &= check(builder.code[0].op == Op::LoadParam && builder.code[0].comps == 3,
					"Gravity reads the Accel parameter as float3");
			}

			{
				ModuleEntry entry{ &builtin::linearForce(), { 5, 5 }, {} };
				ProgramBuilder builder(&layout);
				entry.module->emit(builder, entry);
				ok &= check(builder.valid(), "LinearForce emits");
				ok &= check(builder.params.size() == 1 && builder.params[0].comps == 3,
					"LinearForce declares a float3 force");
			}

			{
				ModuleEntry entry{ &builtin::pointForce(), { 6, 6 }, {} };
				ProgramBuilder builder(&layout);
				entry.module->emit(builder, entry);
				bool hasNormalize = false;
				for (const Instruction& instruction : builder.code)
					hasNormalize |= instruction.op == Op::Normalize;
				ok &= check(builder.valid(), "PointForce emits");
				ok &= check(hasNormalize, "PointForce normalizes the radial direction");
			}

			{
				ModuleEntry entry{ &builtin::shapeLocation(), { 8, 8 }, {} };
				ProgramBuilder builder(&layout);
				entry.module->emit(builder, entry);
				bool hasRandom = false;
				bool hasCompose = false;
				bool hasSqrt = false;
				for (const Instruction& instruction : builder.code) {
					hasRandom |= instruction.op == Op::Random;
					hasCompose |= instruction.op == Op::Insert;
					hasSqrt |= instruction.op == Op::Sqrt;
				}
				ok &= check(builder.valid(), "ShapeLocation emits");
				ok &= check(builder.params.size() == 2 && builder.params[0].comps == 3 &&
					builder.params[1].comps == 1, "ShapeLocation declares Center and Radius");
				ok &= check(hasRandom && hasCompose && hasSqrt, "ShapeLocation samples a random sphere point");
			}

			{
				ModuleEntry entry{ &builtin::spawnRate(), { 3, 3 }, {} };
				ProgramBuilder builder(&layout);
				entry.module->emit(builder, entry);
				ok &= check(builder.valid(), "SpawnRate emits");
				ok &= check(builder.params.size() == 1 && builder.params[0].comps == 1,
					"SpawnRate declares one scalar parameter");
			}

			{
				ModuleEntry entry{ &builtin::lifetime(), { 4, 4 }, {} };
				ProgramBuilder builder(&layout);
				entry.module->emit(builder, entry);
				ok &= check(builder.valid(), "Lifetime emits");
			}

			{
				ProgramBuilder builder(&layout);
				const Reg position = builder.loadAttr("Position");
				builder.storeAttr("Position", position);
				const Reg again = builder.loadAttr("Position");
				ok &= check(builder.valid() && again.offset == position.offset,
					"store then load forwards within the stage");
			}

			return ok;
		}

		EmitterAsset makeM0Asset() {
			EmitterAsset asset;
			asset.stages[static_cast<uint32_t>(Stage::EmitterSpawn)].push_back({ &builtin::initEmitter(), { 1, 1 }, {} });
			asset.stages[static_cast<uint32_t>(Stage::EmitterUpdate)].push_back({ &builtin::spawnRate(), { 2, 2 }, {} });
			asset.stages[static_cast<uint32_t>(Stage::ParticleSpawn)].push_back({ &builtin::initParticle(), { 3, 3 }, {} });
			asset.stages[static_cast<uint32_t>(Stage::ParticleUpdate)].push_back({ &builtin::gravity(), { 4, 4 }, {} });
			asset.stages[static_cast<uint32_t>(Stage::ParticleUpdate)].push_back({ &builtin::lifetime(), { 5, 5 }, {} });
			return asset;
		}

		bool testRuntime() {
			EmitterAsset asset = makeM0Asset();

			bool ok = check(compileEmitter(asset), "M0 asset compiles");
			if (!ok || !asset.compiled)
				return false;

			for (uint32_t s = 0; s < kStageCount; ++s)
				ok &= check(asset.compiled->stages[s] != nullptr, "every stage compiled");
			ok &= check(asset.compiled->spawnOffsets[0].valid, "SpawnInfo0 offsets resolved");
			ok &= check(!asset.compiled->spawnOffsets[1].valid, "unused spawn slot stays invalid");

			const AttributeDesc* position = asset.particleLayout.find("Position");
			ok &= check(position && position->offset == 0 && position->comps == 3, "Position resolved to offset 0");

			const AttributeDesc* age = asset.particleLayout.find("Age");
			ok &= check(age && age->offset == 6, "Age follows Position and Velocity");

			ParticleSystemInstance system;
			ParticleEmitterInstance& emitter = system.addEmitter(asset);
			ok &= check(emitter.ready(), "emitter initialized");

			const float dt = 1.0f / 60.0f;
			for (int frame = 0; frame < 60; ++frame)
				system.tick(dt);

			{
				const ParticleDataSet& set = emitter.renderDataSet();
				ok &= check(set.num > 0, "particles spawned");
				ok &= check(set.stride == asset.particleLayout.totalComponentsCount(), "stride matches layout");
				ok &= check(std::fabs(*set.attr(6, 0) - 59.0f * dt) < 1e-4f, "oldest particle aged 59 dt");
				ok &= check(*set.attr(2, 0) < -350.0f && *set.attr(2, 0) > -420.0f,
					"gravity default -980 integrated into z");
			}

			for (int frame = 60; frame < 180; ++frame)
				system.tick(dt);

			{
				const ParticleDataSet& set = emitter.renderDataSet();
				ok &= check(set.num >= 198 && set.num <= 202, "steady-state population after 3s");
			}

			// A spawned particle must not be updated on its spawn frame.
			ParticleSystemInstance fresh;
			ParticleEmitterInstance& one = fresh.addEmitter(asset);
			fresh.tick(dt);
			{
				const ParticleDataSet& set = one.renderDataSet();
				ok &= check(set.num >= 1, "first tick spawns");
				ok &= check(*set.attr(6, 0) == 0.0f, "spawned particles keep age 0");
				ok &= check(*set.attr(2, 0) == 0.0f, "spawned particles are not integrated twice");
			}
			fresh.tick(dt);
			{
				const ParticleDataSet& set = one.renderDataSet();
				ok &= check(std::fabs(*set.attr(6, 0) - dt) < 1e-6f, "next tick ages by one dt");
			}

			// A module placed in a stage it does not declare must fail compilation.
			EmitterAsset wrong;
			wrong.stages[static_cast<uint32_t>(Stage::EmitterUpdate)].push_back({ &builtin::gravity(), { 7, 7 }, {} });
			ok &= check(!compileEmitter(wrong), "module in the wrong stage is rejected");

			return ok;
		}

		bool testNewModules() {
			// Every new module in its stage at once: catches emit width errors, illegal
			// implicit reads, and register exhaustion that single-module tests miss.
			EmitterAsset asset = makeM0Asset();

			std::vector<ModuleEntry>& spawn = asset.stages[static_cast<uint32_t>(Stage::ParticleSpawn)];
			spawn.push_back({ &builtin::shapeBox(), { 20, 20 }, {} });
			spawn.push_back({ &builtin::shapeDisc(), { 21, 21 }, {} });
			spawn.push_back({ &builtin::shapeCone(), { 22, 22 }, {} });
			spawn.push_back({ &builtin::shapeRing(), { 23, 23 }, {} });
			spawn.push_back({ &builtin::initialVelocity(), { 24, 24 }, {} });
			spawn.push_back({ &builtin::lifetimeRange(), { 25, 25 }, {} });

			std::vector<ModuleEntry>& update = asset.stages[static_cast<uint32_t>(Stage::ParticleUpdate)];
			update.push_back({ &builtin::drag(), { 26, 26 }, {} });
			update.push_back({ &builtin::speedLimit(), { 27, 27 }, {} });
			update.push_back({ &builtin::vortex(), { 28, 28 }, {} });
			update.push_back({ &builtin::killSphere(), { 29, 29 }, {} });
			update.push_back({ &builtin::killBox(), { 30, 30 }, {} });

			bool ok = check(compileEmitter(asset), "every new module compiles in its stage");
			return ok;
		}

		bool testMotionModules() {
			EmitterAsset asset;
			asset.stages[static_cast<uint32_t>(Stage::EmitterSpawn)].push_back(
				{ &builtin::initEmitter(), { 40, 40 }, {} });
			asset.stages[static_cast<uint32_t>(Stage::EmitterUpdate)].push_back(
				{ &builtin::spawnRate(), { 41, 41 }, { 60.0f } });
			asset.stages[static_cast<uint32_t>(Stage::ParticleSpawn)].push_back(
				{ &builtin::initParticle(), { 42, 42 }, {} });
			asset.stages[static_cast<uint32_t>(Stage::ParticleSpawn)].push_back(
				{ &builtin::initialVelocity(), { 43, 43 }, { 0.0f, 0.0f, 1.0f, 100.0f, 100.0f, 0.0f } });
			asset.stages[static_cast<uint32_t>(Stage::ParticleUpdate)].push_back(
				{ &builtin::lifetime(), { 44, 44 }, {} });
			asset.stages[static_cast<uint32_t>(Stage::ParticleUpdate)].push_back(
				{ &builtin::drag(), { 45, 45 }, { 1.0f } });

			bool ok = check(compileEmitter(asset), "motion modules compile");
			if (!ok || !asset.compiled)
				return false;

			const AttributeDesc* position = asset.particleLayout.find("Position");
			const AttributeDesc* velocity = asset.particleLayout.find("Velocity");
			ok &= check(position && velocity, "motion attributes available");
			if (!position || !velocity)
				return false;

			ParticleSystemInstance system;
			ParticleEmitterInstance& emitter = system.addEmitter(asset);

			// 60/s at dt 0.1 spawns 6 per tick. Spawn happens after ParticleUpdate, so
			// the first update a particle sees is the following tick: speed 100 for one
			// tick, then drag 1.0 scales it to 90 and moves it 9 units up (asset +Z).
			system.tick(0.1f);
			system.tick(0.1f);

			const ParticleDataSet& set = emitter.renderDataSet();
			ok &= check(set.num == 12, "two ticks spawn twelve particles");
			ok &= check(std::fabs(*set.attr(position->offset + 2, 0) - 9.0f) < 1e-3f,
				"InitialVelocity plus Drag move the oldest particle 9 units");
			ok &= check(std::fabs(*set.attr(velocity->offset + 2, 0) - 90.0f) < 1e-3f,
				"Drag of 1.0 at dt 0.1 retains 90% of the speed");

			return ok;
		}

		bool testShapeLocation() {
			EmitterAsset asset = makeM0Asset();
			ModuleEntry sphere;
			sphere.module = &builtin::shapeLocation();
			sphere.guid = { 9, 9 };
			sphere.params = { 10.0f, -20.0f, 30.0f, 250.0f };
			asset.stages[static_cast<uint32_t>(Stage::ParticleSpawn)].push_back(sphere);

			bool ok = check(compileEmitter(asset), "ShapeLocation asset compiles");
			if (!ok || !asset.compiled)
				return false;

			const AttributeDesc* position = asset.particleLayout.find("Position");
			ok &= check(position && position->comps == 3, "Position attribute available");
			if (!position)
				return false;

			ParticleSystemInstance system;
			ParticleEmitterInstance& emitter = system.addEmitter(asset);

			const float dt = 0.1f;
			system.tick(dt);

			const ParticleDataSet& set = emitter.renderDataSet();
			ok &= check(set.num >= 8, "sphere batch spawned");

			const float center[3] = { 10.0f, -20.0f, 30.0f };
			constexpr float radius = 250.0f;
			float maxDistance = 0.0f;
			for (uint32_t i = 0; i < set.num; ++i) {
				const float dx = *set.attr(position->offset + 0, i) - center[0];
				const float dy = *set.attr(position->offset + 1, i) - center[1];
				const float dz = *set.attr(position->offset + 2, i) - center[2];
				const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
				if (distance > maxDistance)
					maxDistance = distance;
				ok &= check(distance <= radius + 1e-2f, "particle stays inside the sphere");
			}
			ok &= check(maxDistance > 0.25f * radius, "sphere positions are spread out");

			// reset() rewinds the seed sequence, so a replay must land identically.
			const float firstX = *set.attr(position->offset + 0, 0);
			const float firstY = *set.attr(position->offset + 1, 0);
			const float firstZ = *set.attr(position->offset + 2, 0);
			emitter.reset();
			system.tick(dt);

			const ParticleDataSet& replay = emitter.renderDataSet();
			ok &= check(replay.num >= 1, "reset respawns");
			if (replay.num >= 1) {
				ok &= check(*replay.attr(position->offset + 0, 0) == firstX &&
					*replay.attr(position->offset + 1, 0) == firstY &&
					*replay.attr(position->offset + 2, 0) == firstZ,
					"reset replays the spawn random sequence");
			}

			return ok;
		}

	}

	bool ParticleSelfTest::runAll() {
		bool ok = true;
		ok &= testRegistry();
		ok &= testDataSet();
		ok &= testVM();
		ok &= testModules();
		ok &= testRuntime();
		ok &= testNewModules();
		ok &= testMotionModules();
		ok &= testShapeLocation();

		if (ok)
			VE_CORE_SUCCESS_PRINT("%s", "ParticleSelfTest passed");
		return ok;
	}

}

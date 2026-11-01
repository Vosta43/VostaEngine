#include "vepch.h"
#include "EmitterCompiler.h"

#include "AttributeRegistry.h"
#include "Core/Log.h"
#include "ProgramBuilder.h"

namespace ve {

	namespace {

		constexpr uint64_t kFnvOffset = 1469598103934665603ull;
		constexpr uint64_t kFnvPrime = 1099511628211ull;

		void hashMix(uint64_t& hash, uint64_t value) {
			for (int i = 0; i < 8; ++i) {
				hash ^= (value >> (i * 8)) & 0xFFull;
				hash *= kFnvPrime;
			}
		}

		void hashString(uint64_t& hash, const std::string& text) {
			for (unsigned char c : text) {
				hash ^= c;
				hash *= kFnvPrime;
			}
			hash ^= 0xFFull;
			hash *= kFnvPrime;
		}

		const char* stageName(Stage stage) {
			switch (stage) {
			case Stage::EmitterSpawn:   return "EmitterSpawn";
			case Stage::EmitterUpdate:  return "EmitterUpdate";
			case Stage::ParticleSpawn:  return "ParticleSpawn";
			case Stage::ParticleUpdate: return "ParticleUpdate";
			}
			return "Unknown";
		}

	}

	uint64_t computeEmitterStructureHash(const EmitterAsset& asset) {
		uint64_t hash = kFnvOffset;
		for (uint32_t s = 0; s < kStageCount; ++s) {
			hashMix(hash, s);
			for (const ModuleEntry& entry : asset.stages[s]) {
				hashMix(hash, entry.guid.a);
				hashMix(hash, entry.guid.b);
				hashMix(hash, entry.module ? entry.module->version : 0);
				if (!entry.module)
					continue;
				for (const ModuleParamDecl& decl : entry.module->params)
					hashString(hash, decl.name);
			}
		}
		return hash;
	}

	bool compileEmitter(EmitterAsset& asset) {
		const uint64_t hash = computeEmitterStructureHash(asset);
		auto compiled = std::make_shared<EmitterCompiled>();
		compiled->structureHash = hash;

		for (uint32_t s = 0; s < kStageCount; ++s) {
			const Stage stage = static_cast<Stage>(s);
			const std::vector<ModuleEntry>& entries = asset.stages[s];
			if (entries.empty())
				continue;

			for (const ModuleEntry& entry : entries) {
				if (!entry.module || !entry.module->emit) {
					VE_CORE_ERROR_PRINT("Particle: %s contains an empty module", stageName(stage));
					return false;
				}
				// For extremely special cases.
				if ((entry.module->validStages & stageBit(stage)) == 0) {
					VE_CORE_ERROR_PRINT("Particle: module '%s' cannot run in %s",
						entry.module->name.c_str(), stageName(stage));
					return false;
				}
			}

			AttributeRegistry& layout = isParticleStage(stage) ? asset.particleLayout : asset.emitterLayout;
			ProgramBuilder builder(&layout);
			for (const ModuleEntry& entry : entries)
				entry.module->emit(builder, entry);

			Program program;
			if (!builder.valid() || !builder.finalize(stage, layout, program)) {
				VE_CORE_ERROR_PRINT("Particle: compiling %s failed: %s",
					stageName(stage), builder.error().c_str());
				return false;
			}

			program.structureHash = hash;
			compiled->stages[s] = std::make_shared<const Program>(std::move(program));
		}

		const AttributeDesc* position = asset.particleLayout.find("Position");
		const AttributeDesc* age = asset.particleLayout.find("Age");
		const AttributeDesc* lifetime = asset.particleLayout.find("Lifetime");

		ParticlePropertiesOffsetsLayout& properties = compiled->particleProperties;
		properties.valid = position && position->comps >= 3;
		if (properties.valid)
			properties.position = position->offset;
		if (age && lifetime) {
			properties.hasFade = true;
			properties.age = age->offset;
			properties.lifetime = lifetime->offset;
		}
		// Optional: a module that feeds a sprite sheet writes this, and the sprite
		// renderer uses it as the tile index.
		if (const AttributeDesc* subUV = asset.particleLayout.find("SubUVIndex")) {
			properties.hasSubUVIndex = true;
			properties.subUVIndex = subUV->offset;
		}

		for (uint32_t slot = 0; slot < kMaxSpawnSlots; ++slot) {
			const std::string prefix = "SpawnInfo" + std::to_string(slot) + ".";
			const AttributeDesc* count = asset.emitterLayout.find(prefix + "Count");
			const AttributeDesc* start = asset.emitterLayout.find(prefix + "InterpStartDt");
			const AttributeDesc* interval = asset.emitterLayout.find(prefix + "IntervalDt");
			const AttributeDesc* group = asset.emitterLayout.find(prefix + "SpawnGroup");

			SpawnInfoOffsets& offsets = compiled->spawnOffsets[slot];
			offsets.valid = count && start && interval && group;
			if (!offsets.valid)
				continue;
			offsets.count = count->offset;
			offsets.interpStartDt = start->offset;
			offsets.intervalDt = interval->offset;
			offsets.spawnGroup = group->offset;
		}

		asset.compiled = compiled;
		asset.structureHash = hash;
		return true;
	}

}

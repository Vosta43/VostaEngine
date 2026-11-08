#pragma once

#include "AttributeRegistry.h"
#include "ModuleAsset.h"
#include "Program.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ve {

	struct ModuleEntry {
		const ModuleAsset* module = nullptr;
		Guid16 guid;
		std::vector<float> params;
	};

	// How a simulation's particles become geometry. Sprite is the only kind so far;
	// the tag is persisted so the file can gain other renderers later.
	enum class ParticleRendererType : uint8_t {
		Sprite = 0
	};

	struct ParticleRenderer {
		ParticleRendererType type = ParticleRendererType::Sprite;

		// Project-relative path of the sprite texture. Empty draws the built-in
		// soft dot, so assets authored before the renderer section existed keep
		// their look.
		std::string spriteTexturePath;

		// Sprite-sheet grid; 1x1 samples the whole texture. Particles pick a tile
		// through their optional SubUVIndex attribute.
		uint32_t subImageSizeX = 1;
		uint32_t subImageSizeY = 1;
	};

	struct SpawnInfoOffsets {
		uint16_t count = 0;
		uint16_t interpStartDt = 0;
		uint16_t intervalDt = 0;
		uint16_t spawnGroup = 0;
		bool valid = false;
	};

	struct ParticlePropertiesOffsetsLayout {
		uint16_t position = 0;
		uint16_t age = 0;
		uint16_t lifetime = 0;
		uint16_t subUVIndex = 0;
		uint16_t size = 0;
		uint16_t color = 0;
		uint16_t emissive = 0;
		bool valid = false;
		bool hasFade = false;
		bool hasSubUVIndex = false;
		bool hasSize = false;
		bool hasColor = false;
		bool hasEmissive = false;
	};

	struct EmitterCompiled {
		std::shared_ptr<const Program> stages[kStageCount];
		SpawnInfoOffsets spawnOffsets[kMaxSpawnSlots];      // resolved once per asset
		ParticlePropertiesOffsetsLayout particleProperties; // resolved once per asset
		uint64_t structureHash = 0;
	};

	struct EmitterAsset {
		std::vector<ModuleEntry> stages[kStageCount];
		AttributeRegistry emitterLayout;
		AttributeRegistry particleLayout;
		ParticleRenderer renderer;
		uint64_t structureHash = 0;
		std::shared_ptr<const EmitterCompiled> compiled;
	};

	inline const ModuleParamDecl* findParamDecl(const ModuleAsset& module, const std::string& name) {
		for (const ModuleParamDecl& decl : module.params) {
			if (decl.name == name)
				return &decl;
		}
		return nullptr;
	}

	// Component offset of a declared parameter inside ModuleEntry::params.
	inline uint16_t paramComponentOffset(const ModuleAsset& module, const std::string& name) {
		uint16_t offset = 0;
		for (const ModuleParamDecl& decl : module.params) {
			if (decl.name == name)
				return offset;
			offset = static_cast<uint16_t>(offset + decl.comps);
		}
		return offset;
	}

}

#pragma once

#include "ParticleTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ve {

	class ProgramBuilder;
	struct ModuleEntry;

	struct ModuleParamDecl {
		std::string name;
		float value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		uint8_t comps = 1;
	};

	struct ModuleAsset {
		std::string name;
		uint32_t version = 1;
		uint8_t validStages = 0;
		std::vector<ModuleParamDecl> params;
		void (*emit)(ProgramBuilder& builder, const ModuleEntry& entry) = nullptr;
	};

}

#pragma once

#include "Core/Core.h"
#include "EmitterAsset.h"

#include <cstdint>

namespace ve {

	// Ordered (guid, module version, parameter names); excludes parameter values.
	VE_API uint64_t computeEmitterStructureHash(const EmitterAsset& asset);

	// Compiles all four stages in fixed order. On failure the asset keeps its previous program.
	VE_API bool compileEmitter(EmitterAsset& asset);

}

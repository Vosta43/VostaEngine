#pragma once

#include "Core/Core.h"
#include "ModuleAsset.h"

#include <vector>

namespace ve::builtin {

	VE_API const ModuleAsset& gravity();
	VE_API const ModuleAsset& linearForce();
	VE_API const ModuleAsset& pointForce();
	VE_API const ModuleAsset& initEmitter();
	VE_API const ModuleAsset& initParticle();
	VE_API const ModuleAsset& shapeLocation();
	VE_API const ModuleAsset& spawnRate();
	VE_API const ModuleAsset& lifetime();

	// Every builtin module, for asset loading and the editor's add-module menu.
	VE_API const std::vector<const ModuleAsset*>& builtinModules();

}

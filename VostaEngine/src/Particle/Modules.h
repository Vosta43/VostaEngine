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
	VE_API const ModuleAsset& shapeBox();
	VE_API const ModuleAsset& shapeDisc();
	VE_API const ModuleAsset& shapeCone();
	VE_API const ModuleAsset& shapeRing();
	VE_API const ModuleAsset& initialVelocity();
	VE_API const ModuleAsset& lifetimeRange();
	VE_API const ModuleAsset& spawnRate();
	VE_API const ModuleAsset& lifetime();
	VE_API const ModuleAsset& drag();
	VE_API const ModuleAsset& speedLimit();
	VE_API const ModuleAsset& vortex();
	VE_API const ModuleAsset& killSphere();
	VE_API const ModuleAsset& killBox();

	// Every builtin module, for asset loading and the editor's add-module menu.
	VE_API const std::vector<const ModuleAsset*>& builtinModules();

}

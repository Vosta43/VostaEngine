#pragma once

#include "Core/Core.h"
#include "EmitterAsset.h"

#include <cstdint>
#include <string>

namespace ve {

	// v2 appended the renderer section; v1 files load with the default renderer.
	constexpr int32_t kEmitterFormatVersion = 2;

	// Default M0 stack: InitEmitter, SpawnRate, InitParticle, Gravity, Lifetime.
	// Not compiled; call compileEmitter() before handing it to an instance.
	VE_API EmitterAsset makeDefaultEmitterAsset();

	// Entry with a fresh GUID and params initialized from the module's declared defaults.
	VE_API ModuleEntry createModuleEntry(const ModuleAsset& module);

	// Repairs zero or duplicate GUIDs. Loading re-runs it over saved entries, and the
	// editor runs it after adding a module so two same-kind modules stay distinguishable.
	VE_API void ensureUniqueModuleGuids(EmitterAsset& asset);

	VE_API bool saveEmitterAsset(const EmitterAsset& asset, const std::string& path);

	// Parses and resolves module names into builtin pointers; never compiles.
	// `warning` collects skipped unknown/mismatched modules. `unsupportedVersion` is
	// set when the file is a newer format -- the caller must not overwrite it.
	VE_API bool loadEmitterAsset(const std::string& path, EmitterAsset& out,
		std::string* error = nullptr, std::string* warning = nullptr,
		bool* unsupportedVersion = nullptr);

	VE_API bool writeDefaultEmitterAsset(const std::string& path);

}

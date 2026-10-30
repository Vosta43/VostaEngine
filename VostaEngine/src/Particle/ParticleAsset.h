#pragma once

#include "Core/Core.h"
#include "EmitterAsset.h"

#include <cstdint>
#include <string>

namespace ve {

	// ResourceManager wrapper around an emitter asset: loads a .veasset particle
	// effect and compiles it, so scene components can reference it by handle like
	// any other asset.
	struct VE_API ParticleAsset {
		EmitterAsset emitter;
		// Bumped on every in-place refresh so a running instance can pick up edits.
		uint64_t revision = 0;

		static Ref<ParticleAsset> create(const std::string& path);

		// Re-reads a registered asset in place after the particle editor saved it.
		// No-op when the path is not loaded, or the new file fails to compile.
		static void refreshFromFile(const std::string& path);
	};

}

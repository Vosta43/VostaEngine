#pragma once

#include "Core/Core.h"
#include "ParticleDataSet.h"
#include "Program.h"

#include <cstdint>

namespace ve {

	struct ExecContext {
		const Program* program = nullptr;
		const ParticleDataSet* src = nullptr;
		ParticleDataSet* dst = nullptr;
		uint32_t srcIndex = 0;
		uint32_t dstIndex = 0;
		float dt = 0.0f;
		float spawnTime = 0.0f;
		uint32_t spawnIndex = 0;
		uint32_t spawnCount = 0;
		uint32_t spawnGroup = 0;
		float spawnRandom = 0.0f;   // fresh per-particle seed, never reused between spawns
		const float* paramBlock = nullptr;
	};

	class VE_API VM {
	public:
		static bool run(const ExecContext& context);
	};

}

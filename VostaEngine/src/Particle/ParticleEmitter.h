#pragma once

#include "Core/Core.h"
#include "EmitterAsset.h"
#include "ParticleDataSet.h"
#include "ParticleTypes.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace ve {

	// One emitter instance: own particle state, programs shared with the asset.
	class VE_API ParticleEmitterInstance {
	public:
		// Requires asset.compiled; call compileEmitter() first.
		bool initialize(const EmitterAsset& asset);
		void tick(float dt);
		void reset();
		// Pushes edited parameter values into the running instance without resetting
		// particles. False when the asset's compiled structure no longer matches.
		bool applyParams(const EmitterAsset& asset);

		bool ready() const { return m_compiled != nullptr; }
		const ParticleDataSet& renderDataSet() const { return m_particles[m_cur]; }

	private:
		void initializeEmitter();
		void updateEmitter(float dt);
		void initializeParticle(float dt);
		void updateParticle(float dt);	
		void commit();

		void rebuildParamBlock(const EmitterAsset& asset);
		// A fresh [0,1) seed per spawned particle; reset() restarts the sequence so
		// a replay produces identical particles.
		float nextSpawnRandom();
		SpawnInfo readSpawnInfo(uint32_t slot) const;
		void runEmitterStage(Stage stage, float dt);
		const Program* stageProgram(Stage stage) const;

		std::shared_ptr<const EmitterCompiled> m_compiled;

		ParticleDataSet m_particles[2];
		uint32_t m_cur = 0;
		ParticleDataSet m_emitter;

		// Parameter offsets are program-local, so each stage gets its own flat block.
		std::vector<float> m_paramBlock[kStageCount];

		bool m_spawnDone = false;
		float m_emitterAge = 0.0f;
		uint64_t m_rng = 0;
	};

}

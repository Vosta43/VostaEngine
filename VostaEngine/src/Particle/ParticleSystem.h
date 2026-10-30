#pragma once

#include "Core/Core.h"
#include "ParticleEmitter.h"

#include <vector>

namespace ve {

	// One system instance: owns its emitters and drives their tick order.
	class VE_API ParticleSystemInstance {
	public:
		ParticleSystemInstance() = default;
		// dllexport forces MSVC to instantiate the implicit copy assignment, which
		// cannot exist with unique_ptr-owned emitters. Delete it explicitly.
		ParticleSystemInstance(const ParticleSystemInstance&) = delete;
		ParticleSystemInstance& operator=(const ParticleSystemInstance&) = delete;
		ParticleSystemInstance(ParticleSystemInstance&&) = default;
		ParticleSystemInstance& operator=(ParticleSystemInstance&&) = default;

		ParticleEmitterInstance& addEmitter(EmitterAsset& asset);
		void tick(float dt);
		void reset();

		const std::vector<Scope<ParticleEmitterInstance>>& emitters() const { return m_emitters; }

	private:
		std::vector<Scope<ParticleEmitterInstance>> m_emitters;
	};

}

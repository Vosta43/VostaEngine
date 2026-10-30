#include "vepch.h"
#include "ParticleSystem.h"

#include "Core/Log.h"
#include "EmitterCompiler.h"

namespace ve {

	ParticleEmitterInstance& ParticleSystemInstance::addEmitter(EmitterAsset& asset) {
		if (!asset.compiled && !compileEmitter(asset))
			VE_CORE_ERROR_PRINT("ParticleSystemInstance: emitter asset failed to compile");

		Scope<ParticleEmitterInstance> emitter = CreateScope<ParticleEmitterInstance>();
		emitter->initialize(asset);
		m_emitters.push_back(std::move(emitter));
		return *m_emitters.back();
	}

	void ParticleSystemInstance::tick(float dt) {
		for (Scope<ParticleEmitterInstance>& emitter : m_emitters)
			emitter->tick(dt);
	}

	void ParticleSystemInstance::reset() {
		for (Scope<ParticleEmitterInstance>& emitter : m_emitters)
			emitter->reset();
	}

}

#include "vepch.h"
#include "Scene/Components.h"

#include "Particle/EmitterAssetIO.h"
#include "Particle/EmitterCompiler.h"
#include "Particle/ParticleAsset.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"

namespace ve {

	namespace {

		// What an entity with no assigned asset emits: the same default stack the
		// particle editor opens on a new file.
		EmitterAsset& defaultEmitter() {
			static EmitterAsset asset = makeDefaultEmitterAsset();
			static const bool compiled = compileEmitter(asset);
			(void)compiled;
			return asset;
		}

	}

	void ParticleComponent::tick(float dt) {
		if (!playing)
			return;

		if (!m_system || m_builtHandle != assetHandle) {
			rebuild();
			if (!m_system)
				return;
		}
		else if (m_builtFallback) {
			// Running the built-in emitter because the assigned asset was not
			// resident; take it over as soon as it resolves.
			if (ResourceManager::get<ParticleAsset>(assetHandle)) {
				rebuild();
				if (!m_system)
					return;
			}
		}
		else if (m_asset && m_asset->revision != m_builtRevision) {
			// The particle editor rewrites the asset in place while the scene runs;
			// push the edited parameters through instead of restarting the effect.
			m_builtRevision = m_asset->revision;
			bool restarted = false;
			for (const Scope<ParticleEmitterInstance>& emitter : m_system->emitters()) {
				if (!emitter->applyParams(m_asset->emitter)) {
					rebuild();
					restarted = true;
					break;
				}
			}
			if (restarted && !m_system)
				return;
		}

		m_system->tick(dt * timeScale);
	}

	void ParticleComponent::rebuild() {
		resetRuntime();
		m_builtHandle = assetHandle;

		if (assetHandle.isValid()) {
			m_asset = ResourceManager::get<ParticleAsset>(assetHandle);
			if (!m_asset) {
				// Not resident yet (still streaming in) or a stale assignment. Play
				// the built-in stack rather than nothing; tick() retries.
				m_builtFallback = true;
				VE_CORE_WARN_PRINT("ParticleComponent: assigned particle asset unavailable, using the built-in emitter");
			}
		}

		m_system = CreateScope<ParticleSystemInstance>();
		m_system->addEmitter(m_asset ? m_asset->emitter : defaultEmitter());
		m_builtRevision = m_asset ? m_asset->revision : 0;
	}

	const EmitterAsset* ParticleComponent::emitterAsset() const {
		if (!m_system)
			return nullptr;
		return m_asset ? &m_asset->emitter : &defaultEmitter();
	}

	const ParticleDataSet* ParticleComponent::particleData() const {
		if (!m_system || m_system->emitters().empty())
			return nullptr;
		return &m_system->emitters().front()->renderDataSet();
	}

}

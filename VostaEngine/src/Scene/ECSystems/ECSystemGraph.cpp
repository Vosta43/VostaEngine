#include "vepch.h"
#include "ECSystemGraph.h"


namespace ve {
	void ECSytemGraph::addSystem(const Ref<ECSystemBase>& system){

		m_systems.push_back(system);
	}
	void ECSytemGraph::removeSystem(const Ref<ECSystemBase>& system){

		for (auto it = m_systems.begin(); it != m_systems.end(); ++it) {
			if (*it == system) {
				m_systems.erase(it);
				break;
			}
		}
	}
	void ECSytemGraph::onUpdate(float deltaTime){

		// Snapshot: a system may add or remove systems while it runs.
		const std::vector<Ref<ECSystemBase>> systems = m_systems;
		for (const auto& sys : systems) {
			sys->onUpdate(deltaTime);
		}
	}
}


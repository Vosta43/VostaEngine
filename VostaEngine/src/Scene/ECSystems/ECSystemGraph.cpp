#include "vepch.h"
#include "ECSystemGraph.h"


namespace ve {
	void ECSytemGraph::addSystem(const Ref<ECSystemBase>& system){

		m_systems.push_back(system);
	}
	void ECSytemGraph::onUpdate(float deltaTime){

		for (const auto& sys : m_systems) {
			sys->onUpdate(deltaTime);
		}
	}
}


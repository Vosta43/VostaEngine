#include "vepch.h"
#include "ECSystemGraph.h"


namespace ve {
	void ECSytemGraph::addSystem(const Ref<ECSystemBase>& system){

		m_systems.push_back(system);
	}
	void ECSytemGraph::onUpdate(float deltaTime){
		
		if (m_systems.empty()) {
			VE_CORE_ERROR_PRINT("ECSystems empty");
		}

		for (const auto& sys : m_systems) {
			sys->onUpdate(deltaTime);
		}
	}
}


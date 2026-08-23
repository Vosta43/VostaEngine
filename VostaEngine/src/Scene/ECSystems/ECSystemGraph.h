#pragma once

#include "ECSystem.h"

#include <vector>


namespace ve {

	// ECSystems' main update manager

	class ECSytemGraph {
	public:
		ECSytemGraph() = default;

		void addSystem(const Ref<ECSystemBase>& system);
		void onUpdate(float deltaTime);

	private:
		std::vector<Ref<ECSystemBase>> m_systems;		
	};


}
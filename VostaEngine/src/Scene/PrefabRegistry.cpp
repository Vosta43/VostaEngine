#include "vepch.h"
#include "Scene/PrefabRegistry.h"

namespace ve {

	PrefabRegistry& PrefabRegistry::get() {
		static PrefabRegistry instance;
		return instance;
	}

	void PrefabRegistry::add(PrefabInfo prefab) {
		m_prefabs.push_back(std::move(prefab));
	}

	const std::vector<PrefabInfo>& PrefabRegistry::list() const {
		return m_prefabs;
	}

	const PrefabInfo* PrefabRegistry::find(const std::string& key) const {
		for (const auto& prefab : m_prefabs)
			if (prefab.key == key)
				return &prefab;
		return nullptr;
	}

	Entity PrefabRegistry::spawn(const std::string& key, Scene& scene) const {
		const PrefabInfo* prefab = find(key);
		if (!prefab)
			return Entity{};
		return prefab->spawn(scene);
	}

}

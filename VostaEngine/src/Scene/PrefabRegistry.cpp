#include "vepch.h"
#include "Scene/PrefabRegistry.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

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

	Entity PrefabRegistry::spawnAt(const std::string& key, Scene& scene, const Vec3& position) const {
		const PrefabInfo* prefab = find(key);
		if (!prefab)
			return Entity{};

		Entity entity = prefab->spawn(scene);
		if (!prefab->placeable || entity.getId() == 0xFFFFFFFFu
			|| !scene.getRegistry().has<TransformComponent>(entity))
			return entity;

		TransformComponent& transform = scene.getComponent<TransformComponent>(entity);
		if (transform.position != Vec3(0.0))
			return entity;
		transform.position = position;
		return entity;
	}

}

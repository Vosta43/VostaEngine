#pragma once

#include "Core/Core.h"
#include "Core/Types.h"
#include "Scene/Entity.h"

#include <functional>
#include <string>
#include <vector>

namespace ve {

	class Scene;

	// One spawnable entity template: a stable `key` for the agent, a `displayName`
	// for menus, a `category` for grouping, and the `spawn` that builds it.
	struct PrefabInfo {
		std::string key;
		std::string displayName;
		std::string category;
		// Builds the entity in `scene` and returns it. Ensure-style prefabs (the
		// terrain system) may return an existing entity rather than a new one.
		std::function<Entity(Scene&)> spawn;
		// Whether spawnAt() may move the result. False for prefabs whose transform
		// is semantic -- the atmosphere's planet centre, a terrain tile's grid slot
		// (tile (0,0) legitimately sits at the origin).
		bool placeable = true;
	};

	// The named entity templates the editor's Create menu and the agent both build
	// from, so "a Light" has exactly one definition.
	//
	// SINGLE INSTANCE ACROSS THE DLL BOUNDARY, same as ToolRegistry: get() is
	// DECLARED here and DEFINED in the .cpp. Built-in prefabs register into the
	// DLL's table and the editor reads them through this imported symbol; an inline
	// body would give the editor its own, empty table.
	class VE_API PrefabRegistry {
	public:
		static PrefabRegistry& get();

		void add(PrefabInfo prefab);
		const std::vector<PrefabInfo>& list() const;
		// The prefab registered under `key`, or null.
		const PrefabInfo* find(const std::string& key) const;
		// Spawn by key into `scene`. Returns an invalid Entity (id 0xFFFFFFFF) for
		// an unknown key.
		Entity spawn(const std::string& key, Scene& scene) const;

		// Spawns like spawn(), then moves the result to `position` (chart space),
		// unless the prefab is not placeable (see PrefabInfo) or placed its own
		// transform.
		Entity spawnAt(const std::string& key, Scene& scene, const Vec3& position) const;

	private:
		std::vector<PrefabInfo> m_prefabs;
	};

	// A file-scope instance registers a prefab at static-init time, mirroring
	// ToolRegistrar / VECOMPONENT.
	struct PrefabRegistrar {
		PrefabRegistrar(const char* key, const char* displayName, const char* category,
						std::function<Entity(Scene&)> spawn, bool placeable = true) {
			PrefabRegistry::get().add({ key, displayName, category, std::move(spawn), placeable });
		}
	};

}

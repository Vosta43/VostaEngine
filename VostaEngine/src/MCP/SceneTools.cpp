#include "vepch.h"
#include "MCP/ToolRegistry.h"

#include "Asset/AssetLibrary.h"
#include "Core/ComponentRegistry.h"
#include "Scene/Components.h"
#include "Scene/PrefabRegistry.h"
#include "Scene/SceneSerializer.h"

#include <string>

namespace ve {
	namespace {

		// ── shared lookups ─────────────────────────────────────────────────

		// The live scene, or null with the reason left in `error`. Read through the
		// provider every call — the editor swaps the scene on load / new-project.
		Ref<Scene> requireScene(std::string& error) {
			Ref<Scene> scene = ToolRegistry::get().scene();
			if (!scene)
				error = "no scene is open";
			return scene;
		}

		// The entity args["entity"] names, or an invalid handle with the reason in
		// `error`. An invalid handle has id 0xFFFFFFFF.
		Entity requireEntity(const Ref<Scene>& scene, const JsonReader& args, std::string& error) {
			const int id = args.getInt("entity", -1);
			if (id < 0) {
				error = "missing 'entity' (an integer id; read live ids from scene_snapshot)";
				return Entity{};
			}

			// getEntity hands back an invalid handle for an id that is out of range or
			// points at a freed slot, which is exactly the check we want.
			Entity entity = scene->getEntity(static_cast<uint32_t>(id));
			if (entity.getId() == 0xFFFFFFFFu)
				error = "no live entity with id " + std::to_string(id) +
						" (read live ids from scene_snapshot)";
			return entity;
		}

		// ── tools ──────────────────────────────────────────────────────────
		// Each returns a JSON string. On failure: toolError(...).

		std::string listComponentTypes(const JsonReader&) {
			JsonWriter w;
			w.beginArray("component_types", componentRegistry().size());
			for (const auto& info : componentRegistry()) {
				w.beginObject();
				w.set("typeKey", info.typeKey);
				w.set("displayName", info.displayName);
				w.set("category", info.category);
				w.end();
			}
			w.end();
			return w.str();
		}

		std::string listPrefabs(const JsonReader&) {
			JsonWriter w;
			const std::vector<PrefabInfo>& prefabs = PrefabRegistry::get().list();
			w.beginArray("prefabs", prefabs.size());
			for (const auto& prefab : prefabs) {
				w.beginObject();
				w.set("key", prefab.key);
				w.set("displayName", prefab.displayName);
				w.set("category", prefab.category);
				w.end();
			}
			w.end();
			return w.str();
		}

		std::string listAssets(const JsonReader& args) {
			// Rescan every call so assets created since the last one show up.
			AssetLibrary& library = AssetLibrary::get();
			library.refresh();

			const std::string filter = args.getString("type", "");

			JsonWriter w;
			const std::vector<AssetTypeInfo>& types = assetTypes();
			w.beginArray("asset_types", types.size());
			for (const AssetTypeInfo& type : types) {
				w.beginObject();
				w.set("typeKey", type.typeKey);
				w.set("displayName", type.displayName);
				w.set("category", type.category);
				w.end();
			}
			w.end();

			const std::vector<AssetEntry>& assets = library.list();
			w.beginArray("assets", assets.size());
			for (const AssetEntry& asset : assets) {
				if (!filter.empty() && asset.typeKey != filter)
					continue;
				w.beginObject();
				w.set("path", asset.path);
				w.set("name", asset.name);
				w.set("type", asset.typeKey);
				w.set("category", asset.category);
				w.end();
			}
			w.end();
			return w.str();
		}

		std::string sceneSnapshot(const JsonReader&) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			JsonWriter w;
			SceneSerializer(scene).serialize(w);
			return w.str();
		}

		std::string getComponent(const JsonReader& args) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			const std::string typeKey = args.getString("type", "");
			const ComponentTypeInfo* info = findComponentType(typeKey);
			if (!info)
				return toolError("unknown component type '" + typeKey + "' (see list_component_types)");

			Entity entity = requireEntity(scene, args, error);
			if (entity.getId() == 0xFFFFFFFFu)
				return toolError(error);

			EntityRegistry& reg = scene->getRegistry();
			if (!info->present(reg, entity))
				return toolError("entity " + std::to_string(entity.getId()) +
								 " has no '" + typeKey + "' component");

			JsonWriter w;
			info->serialize(reg, entity, w);
			return w.str();
		}

		std::string setComponent(const JsonReader& args) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			const std::string typeKey = args.getString("type", "");
			const ComponentTypeInfo* info = findComponentType(typeKey);
			if (!info)
				return toolError("unknown component type '" + typeKey + "' (see list_component_types)");

			Entity entity = requireEntity(scene, args, error);
			if (entity.getId() == 0xFFFFFFFFu)
				return toolError(error);

			if (!args.has("data"))
				return toolError("missing 'data' object for '" + typeKey + "'");

			// deserialize emplaces when the component is absent, so this doubles as
			// "add component" — the same path SceneSerializer::deserialize uses.
			info->deserialize(scene->getRegistry(), entity, args.child("data"));
			return "{\"ok\":true}";
		}

		// Applies an optional NameComponent from args, shared by both create tools so
		// a requested name lands the same way however the entity was made.
		void applyOptionalName(Scene& scene, Entity entity, const JsonReader& args) {
			const std::string name = args.getString("name", "");
			if (name.empty())
				return;
			EntityRegistry& reg = scene.getRegistry();
			if (reg.has<NameComponent>(entity))
				reg.get<NameComponent>(entity).name = name;
			else
				reg.emplace<NameComponent>(entity, name);
		}

		std::string createEntity(const JsonReader& args) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			// Route through the "empty" prefab so an entity the model creates starts
			// from the same baseline (a TransformComponent) as one made from the
			// editor's Create menu — a component-less entity is what used to crash
			// the gizmo.
			Entity entity = PrefabRegistry::get().spawn("empty", *scene);
			if (entity.getId() == 0xFFFFFFFFu)
				return toolError("internal: the 'empty' prefab is not registered");

			applyOptionalName(*scene, entity, args);
			return "{\"id\":" + std::to_string(entity.getId()) + "}";
		}

		std::string spawnPrefab(const JsonReader& args) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			const std::string key = args.getString("prefab", "");
			const PrefabInfo* prefab = PrefabRegistry::get().find(key);
			if (!prefab)
				return toolError("unknown prefab '" + key + "' (call list_prefabs for the keys)");

			Entity entity = prefab->spawn(*scene);
			if (entity.getId() == 0xFFFFFFFFu)
				return toolError("prefab '" + key + "' produced no entity");

			applyOptionalName(*scene, entity, args);
			return "{\"id\":" + std::to_string(entity.getId()) +
				   ",\"prefab\":\"" + jsonEscape(key) + "\"}";
		}

		std::string deleteEntity(const JsonReader& args) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			Entity entity = requireEntity(scene, args, error);
			if (entity.getId() == 0xFFFFFFFFu)
				return toolError(error);

			scene->destroyEntity(entity);
			return "{\"ok\":true}";
		}

		// ── registration ───────────────────────────────────────────────────
		// File-scope objects; each registers itself when the DLL loads.

		ToolRegistrar reg_listComponentTypes{
			"list_component_types",
			"List every component type this build knows about. Call this first to get a valid 'type' value.",
			R"({"type":"object","properties":{}})",
			listComponentTypes
		};

		ToolRegistrar reg_listPrefabs{
			"list_prefabs",
			"List the spawnable prefabs: named templates of pre-built entities (light, static_mesh, atmosphere, terrain, ...). Call this first to get a valid 'prefab' value for spawn_prefab.",
			R"({"type":"object","properties":{}})",
			listPrefabs
		};

		ToolRegistrar reg_listAssets{
			"list_assets",
			"List the assets in the open project: meshes, materials, textures, noise, terrain data and scenes. The reply also lists every valid asset type. Paths are asset-root-relative and can be assigned directly with set_component. Pass 'type' to keep only one kind.",
			R"SCHEMA({"type":"object","properties":{"type":{"type":"string","description":"optional asset typeKey filter, e.g. 'staticmesh' or 'material' (see asset_types in the reply)"}}})SCHEMA",
			listAssets
		};

		ToolRegistrar reg_sceneSnapshot{
			"scene_snapshot",
			"Return the whole current scene as JSON: every entity and its components. Use it to discover live entity ids.",
			R"({"type":"object","properties":{}})",
			sceneSnapshot
		};

		ToolRegistrar reg_getComponent{
			"get_component",
			"Read one component off one entity. Returns the component's fields as a JSON object.",
			R"({"type":"object","properties":{"entity":{"type":"integer","description":"entity id from scene_snapshot"},"type":{"type":"string","description":"component typeKey from list_component_types"}},"required":["entity","type"]})",
			getComponent
		};

		ToolRegistrar reg_setComponent{
			"set_component",
			"Write one component on one entity, adding it if the entity has none. 'data' takes the same shape get_component returned.",
			R"({"type":"object","properties":{"entity":{"type":"integer","description":"entity id from scene_snapshot"},"type":{"type":"string","description":"component typeKey from list_component_types"},"data":{"type":"object","description":"component fields, as returned by get_component"}},"required":["entity","type","data"]})",
			setComponent
		};

		ToolRegistrar reg_createEntity{
			"create_entity",
			"Create a bare entity that has only a TransformComponent, and return its id. For a ready-to-use object (light, mesh, atmosphere, terrain, ...) prefer spawn_prefab. Add more components with set_component.",
			R"({"type":"object","properties":{"name":{"type":"string","description":"optional NameComponent value"}}})",
			createEntity
		};

		ToolRegistrar reg_spawnPrefab{
			"spawn_prefab",
			"Create an entity from a named prefab — the same templates the editor's Create Entity menu offers, so the result is complete (its transform plus whatever the template implies). Prefer this over create_entity. Get keys from list_prefabs.",
			R"({"type":"object","properties":{"prefab":{"type":"string","description":"prefab key from list_prefabs, e.g. 'light'"},"name":{"type":"string","description":"optional NameComponent value"}},"required":["prefab"]})",
			spawnPrefab
		};

		ToolRegistrar reg_deleteEntity{
			"delete_entity",
			"Delete an entity and every component it owns.",
			R"({"type":"object","properties":{"entity":{"type":"integer","description":"entity id from scene_snapshot"}},"required":["entity"]})",
			deleteEntity
		};

	} // namespace
} // namespace ve

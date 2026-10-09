#include "vepch.h"
#include "MCP/CommandRegistry.h"
#include "MCP/ToolRegistry.h"

#include "Core/Json.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Scene/Terrain/Terrain.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ve {
	namespace {

		// The live scene, or null with the reason left in `error`. Read through the
		// provider every call — the editor swaps the scene on load / new-project.
		Ref<Scene> requireScene(std::string& error) {
			Ref<Scene> scene = ToolRegistry::get().scene();
			if (!scene)
				error = "no scene is open";
			return scene;
		}

		// A rectangle of grid coordinates wider than this in one call is refused.
		// The loop behind it creates one entity per cell, so an unbounded request
		// from a model that misread the units would hang the editor.
		constexpr int64_t kMaxTilesPerCall = 4096;

		size_t tileCount(Scene& scene) {
			size_t n = 0;
			for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
				(void)id;
				++n;
			}
			return n;
		}

		// Reads the grid rectangle every terrain action shares. maxX/maxZ default to
		// minX/minZ so a single cell needs only its corner. Leaves `error` set when
		// the request is malformed or too large.
		bool readTileRect(const JsonReader& params, glm::ivec2& lo, glm::ivec2& hi, std::string& error) {
			if (!params.has("minX") || !params.has("minZ")) {
				error = "'minX' and 'minZ' are required (tile grid coordinates; the tile the editor "
						"starts with is usually 0,0)";
				return false;
			}

			lo = glm::ivec2(params.getInt("minX", 0), params.getInt("minZ", 0));
			hi = glm::ivec2(params.getInt("maxX", lo.x), params.getInt("maxZ", lo.y));

			const glm::ivec2 a = glm::min(lo, hi);
			const glm::ivec2 b = glm::max(lo, hi);
			lo = a;
			hi = b;

			const int64_t cells = (int64_t)(hi.x - lo.x + 1) * (int64_t)(hi.y - lo.y + 1);
			if (cells > kMaxTilesPerCall) {
				error = "that is " + std::to_string(cells) + " tiles; at most " +
						std::to_string(kMaxTilesPerCall) + " are allowed per call. Split it into "
						"several calls";
				return false;
			}
			return true;
		}

		// ── terrain tiles ──────────────────────────────────────────────────

		std::string addTerrainTiles(const JsonReader& params) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			glm::ivec2 lo, hi;
			if (!readTileRect(params, lo, hi, error))
				return toolError(error);

			// A tile's world position is derived from the system's tileSize, so the
			// system has to exist before the tiles are placed; without this the new
			// tiles would sit at the origin until something re-placed them.
			Terrain::ensureSystem(*scene);

			const size_t before = tileCount(*scene);
			Terrain::addTiles(*scene, lo, hi);
			const size_t after = tileCount(*scene);

			return "{\"ok\":true,\"added\":" + std::to_string(after - before) +
				   ",\"total\":" + std::to_string(after) + "}";
		}

		std::string removeTerrainTiles(const JsonReader& params) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			glm::ivec2 lo, hi;
			if (!readTileRect(params, lo, hi, error))
				return toolError(error);

			const size_t before = tileCount(*scene);
			Terrain::removeTiles(*scene, lo, hi);
			const size_t after = tileCount(*scene);

			return "{\"ok\":true,\"removed\":" + std::to_string(before - after) +
				   ",\"total\":" + std::to_string(after) + "}";
		}

		// ── terrain query ──────────────────────────────────────────────────

		std::string queryTerrainHeight(const JsonReader& params) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			if (!params.has("x") || !params.has("z"))
				return toolError("'x' and 'z' are required (world-space coordinates)");

			const float x = params.getFloat("x", 0.0f);
			const float z = params.getFloat("z", 0.0f);

			// Straight down from well above the terrain. raycastScene (not the
			// single-tile raycast) finds whichever tile is under the point, so no
			// tile needs to be selected first.
			constexpr float kHighAbove = 10000.0f;
			const glm::vec3 origin(x, kHighAbove, z);
			const glm::vec3 down(0.0f, -1.0f, 0.0f);

			uint32_t entity = UINT32_MAX;
			glm::vec3 hit(0.0f);
			if (!Terrain::raycastScene(*scene, origin, down, kHighAbove * 2.0f, entity, hit))
				return "{\"hit\":false}";

			return "{\"hit\":true,\"y\":" + std::to_string(hit.y) +
				   ",\"entity\":" + std::to_string(entity) + "}";
		}

		// ── entity lifecycle ───────────────────────────────────────────────

		std::string duplicateEntity(const JsonReader& params) {
			std::string error;
			Ref<Scene> scene = requireScene(error);
			if (!scene)
				return toolError(error);

			const int id = params.getInt("entity", -1);
			if (id < 0)
				return toolError("missing 'entity' (an integer id; read live ids from scene_snapshot)");

			Entity source = scene->getEntity(static_cast<uint32_t>(id));
			if (source.getId() == 0xFFFFFFFFu)
				return toolError("no live entity with id " + std::to_string(id) +
								 " (read live ids from scene_snapshot)");

			Entity copy = scene->duplicateEntity(source);
			if (copy.getId() == 0xFFFFFFFFu)
				return toolError("internal: duplication produced no entity");

			// An explicit name wins; otherwise the copy keeps the source's name.
			const std::string name = params.getString("name", "");
			if (!name.empty()) {
				EntityRegistry& reg = scene->getRegistry();
				if (reg.has<NameComponent>(copy))
					reg.get<NameComponent>(copy).name = name;
				else
					reg.emplace<NameComponent>(copy, name);
			}

			return "{\"id\":" + std::to_string(copy.getId()) + "}";
		}

		// ── command registration ───────────────────────────────────────────
		// File-scope objects; each registers itself when the DLL loads. One entry per
		// operation a component write cannot express (something has to run engine
		// code). Adding a capability here does NOT add a tool -- it grows the
		// catalogue the single `editor_action` tool hands back on request.

		CommandRegistrar reg_addTerrainTiles{
			"add_terrain_tiles",
			"Add terrain tiles over a grid rectangle, creating the scene's terrain system "
			"first if there is none. New tiles bake on the next frame.",
			"minX, minZ (int, required), maxX, maxZ (int, default to minX/minZ)",
			addTerrainTiles
		};

		CommandRegistrar reg_removeTerrainTiles{
			"remove_terrain_tiles",
			"Delete the terrain tiles inside a grid rectangle (empty cells are ignored).",
			"minX, minZ (int, required), maxX, maxZ (int, default to minX/minZ)",
			removeTerrainTiles
		};

		CommandRegistrar reg_queryTerrainHeight{
			"query_terrain_height",
			"World-space ground height at an XZ position -- the y of the terrain surface there. "
			"Use it to sit a placed object on the ground instead of guessing. Returns "
			"{\"hit\":false} when no terrain covers that point.",
			"x, z (float, required) - world-space position",
			queryTerrainHeight
		};

		CommandRegistrar reg_duplicateEntity{
			"duplicate_entity",
			"Copy an entity, components and all, and return the new id. The copy keeps the "
			"source's name unless 'name' is given.",
			"entity (int, required), name (string, optional)",
			duplicateEntity
		};

		// ── the single model-facing tool ───────────────────────────────────

		std::string commandCatalogue() {
			const std::vector<CommandInfo>& commands = CommandRegistry::get().list();
			JsonWriter w;
			w.beginArray("actions", commands.size());
			for (const CommandInfo& command : commands) {
				w.beginObject();
				w.set("action", command.name);
				w.set("description", command.summary);
				w.set("params", command.params);
				w.end();
			}
			w.end();
			return w.str();
		}

		std::string editorAction(const JsonReader& args) {
			const std::string action = args.getString("action", "");
			if (action.empty())
				return toolError("missing 'action' (call editor_action with {\"action\":\"list\"} for the choices)");

			if (action == "list")
				return commandCatalogue();

			const CommandInfo* command = CommandRegistry::get().find(action);
			if (!command)
				return toolError("unknown action '" + action +
								 "' (call editor_action with {\"action\":\"list\"} for the choices)");

			// A command sees only its own parameters, never the action wrapper.
			return command->handler(args.child("params"));
		}

		ToolRegistrar reg_editorAction{
			"editor_action",
			"Run a named engine operation that must execute engine code rather than store a "
			"component field -- load terrain tiles, save the scene, duplicate an entity, and so "
			"on. The set of operations grows without the tool list changing, so ALWAYS discover "
			"them first: call it with {\"action\":\"list\"} to read the current catalogue, then "
			"pass the chosen operation's arguments under 'params'.",
			R"SCHEMA({"type":"object","properties":{"action":{"type":"string","description":"operation name; use \"list\" to read the catalogue"},"params":{"type":"object","description":"operation-specific arguments; see the 'list' reply's params column"}},"required":["action"]})SCHEMA",
			editorAction
		};

	} // namespace
} // namespace ve

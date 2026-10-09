#include "EditorMcpTools.h"

#include "EditorLayer.h"

#include "Core/Log.h"
#include "MCP/CommandRegistry.h"
#include "MCP/ToolDispatchQueue.h"
#include "MCP/ToolRegistry.h"

#include <future>
#include <string>

namespace ve {
	namespace EditorMcpTools {

		namespace {

			// The layer attach() bound, or null. Commands reach the editor only through
			// this pointer, read at call time, so detach() can null it out and no
			// handler ever captures a layer that is about to die.
			EditorLayer* s_layer = nullptr;

			std::string handleSaveScene(const JsonReader&) {
				if (!s_layer)
					return toolError("the editor is not attached");
				if (!s_layer->saveScene())
					return toolError("scene save failed (see the log for the reason)");
				return std::string("{\"ok\":true,\"path\":\"") +
					   jsonEscape(s_layer->currentScenePath().string()) + "\"}";
			}

			// Startup smoke test: proves the tool table is populated at static-init time
			// and that both the direct and queued paths reach the same handlers. Log-only.
			void selfTest() {
				const auto& tools = ToolRegistry::get().list();
				VE_CORE_INFO_PRINT("[MCP] ToolRegistry holds %zu tool(s) at &%p", tools.size(), (void*)&ToolRegistry::get());
				for (const auto& tool : tools)
					VE_CORE_INFO_PRINT("[MCP]   - %s", tool.name.c_str());

				VE_CORE_INFO_PRINT("[MCP] list_component_types -> %s",
								   ToolRegistry::get().invoke("list_component_types", "{}").c_str());
				VE_CORE_INFO_PRINT("[MCP] scene_snapshot -> %s",
								   ToolRegistry::get().invoke("scene_snapshot", "{}").c_str());
				VE_CORE_INFO_PRINT("[MCP] editor_action(list) -> %s",
								   ToolRegistry::get().invoke("editor_action", "{\"action\":\"list\"}").c_str());

				// Queue round-trip. Future is taken by submit; drain() must run before
				// get() or this thread would block on itself.
				std::future<std::string> queued =
					ToolDispatchQueue::get().submit("list_component_types", "{}");
				ToolDispatchQueue::get().drain();
				VE_CORE_INFO_PRINT("[MCP] queued round-trip -> %s", queued.get().c_str());
			}

		}

		void attach(EditorLayer& layer) {
			s_layer = &layer;

			// Re-read through the provider on every call, so a scene swapped in by
			// load / new-project is picked up without re-registering.
			ToolRegistry::get().setSceneProvider([]() -> Ref<Scene> {
				return s_layer ? s_layer->currentScene() : nullptr;
			});

			// The editor's own command. Registered here, not at static-init, because it
			// needs the live layer; removed in detach() so it cannot outlive it.
			CommandRegistry::get().add({
				"save_scene",
				"Save the open scene to disk exactly like File > Save: the .veworld, its "
				"terrain-data sidecar and the viewport camera. Agent edits live only in memory "
				"until this runs, and are lost when the editor closes.",
				"no parameters",
				handleSaveScene
			});

			selfTest();
		}

		void detach() {
			ToolRegistry::get().setSceneProvider({});
			CommandRegistry::get().remove("save_scene");
			s_layer = nullptr;
		}

		void pump() {
			ToolDispatchQueue::get().drain();
		}

	}
}

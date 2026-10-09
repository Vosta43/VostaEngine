#pragma once

#include "Core/Core.h"
#include "Core/Json.h"

#include <functional>
#include <string>
#include <vector>

namespace ve {

	// One named engine operation an agent can run: the imperative counterpart to a
	// component write. A component write only stores fields; a command runs code
	// (bake a tile, save the scene, duplicate an entity). `summary` and `params` are
	// the model's only guidance, and the handler receives just the call's `params`
	// object, not the whole argument.
	struct CommandInfo {
		std::string name;
		std::string summary;
		std::string params;
		std::function<std::string(const JsonReader&)> handler;
	};

	// name -> command table. This is the command plane of the tool layer: many small
	// capabilities live here behind ONE model-facing tool (editor_action), so the
	// tool count stays fixed while capabilities grow. Any subsystem registers its own
	// commands -- engine code at static-init time via CommandRegistrar, editor code
	// at runtime -- and the model discovers them by asking for the catalogue.
	//
	// Holds no engine knowledge: every capability lives in the handlers, exactly like
	// ToolRegistry.
	//
	// SINGLE INSTANCE ACROSS THE DLL BOUNDARY. get() is DECLARED here and DEFINED in
	// the .cpp only. An inline body would give the editor its own function-local
	// static and therefore a second, empty table. Same rule as ToolRegistry.
	class VE_API CommandRegistry {
	public:
		static CommandRegistry& get();

		// Registers a command. A second registration under the same name replaces the
		// first, so an editor layer re-attached after a detach cannot leave a stale
		// handler behind.
		void add(CommandInfo command);

		// Drops the command under `name`, if any. The editor uses this on detach to
		// remove a handler that captures editor state before that state dies.
		void remove(const std::string& name);

		const std::vector<CommandInfo>& list() const;

		// The command registered under `name`, or null.
		const CommandInfo* find(const std::string& name) const;

	private:
		std::vector<CommandInfo> m_commands;
	};

	// A file-scope instance registers a command at static-init time, mirroring
	// ToolRegistrar / PrefabRegistrar.
	struct CommandRegistrar {
		CommandRegistrar(const char* name, const char* summary, const char* params,
						 std::function<std::string(const JsonReader&)> handler) {
			CommandRegistry::get().add({ name, summary, params, std::move(handler) });
		}
	};

}

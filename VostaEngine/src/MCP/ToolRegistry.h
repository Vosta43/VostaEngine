#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Scene/Scene.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ve {

	// One callable an agent can invoke. `description` and `inputSchema` are the
	// model's ONLY guidance for picking and filling the tool, so a vague
	// description turns into a wrong call. `inputSchema` is an opaque JSON-Schema
	// string — the registry neither validates nor interprets it.
	struct ToolInfo {
		std::string name;
		std::string description;
		std::string inputSchema;
		std::function<std::string(const JsonReader&)> handler;
	};

	// Escape a string so it can sit inside a JSON string literal. Message text can
	// carry a path (backslashes) or a type key (quotes); without this the caller
	// could fail to parse the error it was just handed.
	inline std::string jsonEscape(const std::string& in) {
		std::string out;
		out.reserve(in.size() + 8);
		for (char c : in) {
			switch (c) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:   out += c;      break;
			}
		}
		return out;
	}

	// Build a {"error": "..."} result. Handlers report failure this way so the
	// caller always receives parseable JSON, never an empty or partial string.
	inline std::string toolError(const std::string& message) {
		return "{\"error\":\"" + jsonEscape(message) + "\"}";
	}

	// name -> handler table. Holds no engine knowledge itself: every capability
	// lives in the handlers, and the "operate any component" reach comes from
	// componentRegistry(), not from here. Deliberately protocol-agnostic — an MCP
	// adapter and an in-editor chat both turn list()/invoke() into their own wire
	// format, and swapping protocol touches neither this header nor the tools.
	//
	// SINGLE INSTANCE ACROSS THE DLL BOUNDARY. get() is DECLARED here and DEFINED
	// in the .cpp only. An inline body would give the editor its own function-local
	// static and therefore a second, empty table. Same reason ResourceManager keeps
	// its template bodies in the .cpp.
	class VE_API ToolRegistry {
	public:
		static ToolRegistry& get();

		void add(ToolInfo tool);
		const std::vector<ToolInfo>& list() const;

		// Runs a tool. NEVER throws: an unknown name, unparseable arguments and a
		// throwing handler all come back as {"error": "..."}. The message is meant
		// to be read by a model that will retry, so it should say what to do next,
		// not just what failed.
		std::string invoke(const std::string& name, const std::string& argsJson);

		// The live scene, supplied by the host. The editor owns the scene and swaps
		// it on load / new-project, so handlers must go through this provider on
		// every call rather than capturing a Scene. Hands back a Ref so the caller
		// can pass it straight to SceneSerializer.
		void setSceneProvider(std::function<Ref<Scene>()> provider);
		Ref<Scene> scene() const;

	private:
		std::vector<ToolInfo> m_tools;
		std::function<Ref<Scene>()> m_sceneProvider;
	};

	// A file-scope instance of this registers a tool at static-init time, mirroring
	// the VECOMPONENT idiom. No macro: tools have real function bodies, so a plain
	// constructor taking the handler reads better than one.
	struct ToolRegistrar {
		ToolRegistrar(const char* name, const char* description, const char* inputSchema,
					  std::function<std::string(const JsonReader&)> handler) {
			ToolRegistry::get().add({ name, description, inputSchema, std::move(handler) });
		}
	};

}

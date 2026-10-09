#include "vepch.h"
#include "MCP/ToolRegistry.h"

#include "Core/Log.h"

namespace ve {

	ToolRegistry& ToolRegistry::get() {
		static ToolRegistry instance;
		return instance;
	}

	void ToolRegistry::add(ToolInfo tool) {
		m_tools.push_back(std::move(tool));
	}

	const std::vector<ToolInfo>& ToolRegistry::list() const {
		return m_tools;
	}

	std::string ToolRegistry::invoke(const std::string& name, const std::string& argsJson) {
		const ToolInfo* tool = nullptr;
		for (const auto& candidate : m_tools) {
			if (candidate.name == name) {
				tool = &candidate;
				break;
			}
		}
		if (!tool)
			return toolError("unknown tool: " + name);

		// No arguments is a normal case; an empty string cannot be parsed, so feed
		// the handler an empty object instead of failing.
		const std::string text = argsJson.empty() ? std::string("{}") : argsJson;
		JsonReader args;
		if (!JsonReader::parse(text, args))
			return toolError("invalid JSON arguments for " + name);

		try {
			return tool->handler(args);
		}
		catch (const std::exception& e) {
			return toolError(name + " failed: " + e.what());
		}
		catch (...) {
			return toolError(name + " failed: unknown exception");
		}
	}

	void ToolRegistry::setSceneProvider(std::function<Ref<Scene>()> provider) {
		m_sceneProvider = std::move(provider);
	}

	Ref<Scene> ToolRegistry::scene() const {
		return m_sceneProvider ? m_sceneProvider() : nullptr;
	}

}

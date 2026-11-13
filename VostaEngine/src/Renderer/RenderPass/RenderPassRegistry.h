#pragma once

#include "Core/Core.h"
#include "Renderer/RenderPass/RenderPassBase.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace ve {

	// Maps a PassDef "type" string to a pass factory. Internal to the engine;
	// a project's extensibility point is the declarative "draw" pass, not this.
	class RenderPassRegistry {
	public:
		using Factory = std::function<Ref<RenderPassBase>()>;

		void add(const std::string& type, Factory factory);
		Ref<RenderPassBase> create(const std::string& type) const;

	private:
		std::unordered_map<std::string, Factory> m_factories;
	};

	inline void RenderPassRegistry::add(const std::string& type, Factory factory) {
		m_factories[type] = std::move(factory);
	}

	inline Ref<RenderPassBase> RenderPassRegistry::create(const std::string& type) const {
		auto it = m_factories.find(type);
		return (it != m_factories.end()) ? it->second() : nullptr;
	}

}

#include "vepch.h"
#include "RenderPassBase.h"
#include "Core/Log.h"

namespace ve {

	void RenderPassBase::init()
	{

	}

	void RenderPassBase::configure(const PassDef& def,
	                               const std::unordered_map<std::string, Ref<Framebuffer>>& fbos,
	                               const Ref<Shader>& shader)
	{
		m_def = def;
		m_name = def.name;
		m_type = def.type;
		m_shader = shader;
		m_target.reset();
		m_history[0].reset();
		m_history[1].reset();
		m_written.reset();

		m_targetsDefault = (def.target == "@default");
		if (!m_targetsDefault && !def.target.empty()) {
			auto it = fbos.find(def.target);
			if (it != fbos.end())
				m_target = it->second;
			else
				VE_CORE_WARN_PRINT("RenderPass '%s': target '%s' is not a known framebuffer", m_name.c_str(), def.target.c_str());
		}

		if (def.history.size() >= 2) {
			auto a = fbos.find(def.history[0]);
			auto b = fbos.find(def.history[1]);
			if (a != fbos.end()) m_history[0] = a->second;
			if (b != fbos.end()) m_history[1] = b->second;
			if (!m_history[0] || !m_history[1])
				VE_CORE_WARN_PRINT("RenderPass '%s': incomplete history pair", m_name.c_str());
		}
	}

}

#pragma once

#include "PipelineConfig.h"
#include "Renderer/Shader.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ve {

	// Values a pipeline asset exposes for tweaking. Four apply kinds:
	//   Uniform  - pushed to every pass shader each frame (free to change)
	//   Pass     - gates whether a named pass runs
	//   Resource - scales a named framebuffer (read at FBO build time)
	//   Variant  - modelled but not implemented (no shader recompile in Vosta);
	//              warned about at load time.
	class SettingsRegistry {
	public:
		void load(const std::vector<SettingDef>& defs);

		const std::vector<SettingDef>& defs() const { return m_defs; }

		bool  getBool(const std::string& name, bool def = false) const;
		float getFloat(const std::string& name, float def = 0.0f) const;

		void set(const std::string& name, float value);
		void set(const std::string& name, bool value);

		// Push every Uniform-kind setting to the shader. A setting whose uniform
		// the shader doesn't declare is a no-op (glUniform(-1)).
		void applyTo(const Ref<Shader>& shader) const;

		// True when the named pass is enabled. Passes with no matching Pass-kind
		// setting default to enabled.
		bool passEnabled(const std::string& passName) const;

		// Multiplier for a named framebuffer's size, from a Resource-kind
		// setting. 1.0 when the framebuffer has no such setting.
		float resourceScale(const std::string& fboName) const;

	private:
		SettingDef*       find(const std::string& name);
		const SettingDef* find(const std::string& name) const;

		std::vector<SettingDef>              m_defs;
		std::unordered_map<std::string, size_t> m_index;
	};

}

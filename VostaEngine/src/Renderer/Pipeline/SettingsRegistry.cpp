#include "vepch.h"
#include "SettingsRegistry.h"

namespace ve {

	void SettingsRegistry::load(const std::vector<SettingDef>& defs) {
		m_defs = defs;
		m_index.clear();
		for (size_t i = 0; i < m_defs.size(); ++i)
			m_index[m_defs[i].name] = i;
	}

	SettingDef* SettingsRegistry::find(const std::string& name) {
		auto it = m_index.find(name);
		return (it != m_index.end()) ? &m_defs[it->second] : nullptr;
	}

	const SettingDef* SettingsRegistry::find(const std::string& name) const {
		auto it = m_index.find(name);
		return (it != m_index.end()) ? &m_defs[it->second] : nullptr;
	}

	bool SettingsRegistry::getBool(const std::string& name, bool def) const {
		const SettingDef* s = find(name);
		return s ? s->boolValue : def;
	}

	float SettingsRegistry::getFloat(const std::string& name, float def) const {
		const SettingDef* s = find(name);
		return s ? s->value[0] : def;
	}

	void SettingsRegistry::set(const std::string& name, float value) {
		if (SettingDef* s = find(name)) s->value[0] = value;
	}

	void SettingsRegistry::set(const std::string& name, bool value) {
		if (SettingDef* s = find(name)) s->boolValue = value;
	}

	void SettingsRegistry::applyTo(const Ref<Shader>& shader) const {
		if (!shader) return;
		for (const auto& s : m_defs) {
			if (s.apply != SettingsApply::Uniform || s.uniform.empty()) continue;
			switch (s.kind) {
				case UniformKind::Float: shader->setFloat(s.uniform, s.value[0]); break;
				case UniformKind::Int:   shader->setInt(s.uniform, (int)s.value[0]); break;
				case UniformKind::Bool:  shader->setInt(s.uniform, s.boolValue ? 1 : 0); break;
				case UniformKind::Vec2:  shader->setFloat2(s.uniform, glm::vec2(s.value[0], s.value[1])); break;
				case UniformKind::Vec3:  shader->setFloat3(s.uniform, glm::vec3(s.value[0], s.value[1], s.value[2])); break;
				case UniformKind::Vec4:  shader->setFloat4(s.uniform, glm::vec4(s.value[0], s.value[1], s.value[2], s.value[3])); break;
			}
		}
	}

	bool SettingsRegistry::passEnabled(const std::string& passName) const {
		for (const auto& s : m_defs)
			if (s.apply == SettingsApply::Pass && s.target == passName)
				return s.boolValue;
		return true;
	}

	float SettingsRegistry::resourceScale(const std::string& fboName) const {
		for (const auto& s : m_defs)
			if (s.apply == SettingsApply::Resource && s.target == fboName)
				return s.value[0];
		return 1.0f;
	}

}

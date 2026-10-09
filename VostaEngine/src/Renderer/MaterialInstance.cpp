#include "vepch.h"
#include "MaterialInstance.h"

namespace ve {

	namespace {

		// Replace a binding the base already emitted, or append it when the base
		// never declared that name.
		template <typename T>
		void overrideByName(std::vector<MaterialUniformBinding<T>>& dst,
			const std::vector<MaterialUniformBinding<T>>& src) {
			for (const auto& s : src) {
				bool replaced = false;
				for (auto& d : dst) {
					if (d.name == s.name) {
						d.value = s.value;
						replaced = true;
						break;
					}
				}
				if (!replaced) dst.push_back(s);
			}
		}

		// Setters upsert by name so callers may refresh a value every frame (the
		// terrain instance drives u_TerrainOrigin that way) without the override
		// list growing without bound.
		template <typename T>
		void upsert(std::vector<MaterialUniformBinding<T>>& v, const std::string& name, const T& value) {
			for (auto& e : v) {
				if (e.name == name) { e.value = value; return; }
			}
			v.push_back({ name, value });
		}

	}

	void MaterialInstance::setTexture(const std::string& name, AssetHandle texture, uint32_t unit) {
		for (auto& e : overrides.textures) {
			if (e.uniformName == name) { e.textureHandle = texture; e.unit = unit; return; }
		}
		overrides.textures.push_back({ name, texture, unit });
	}

	void MaterialInstance::fillBindings(MaterialBindingSet& out) const {

		if (base) {
			base->fillBindings(out);  // the base clears the set and declares everything
		}
		else {
			out.textures.clear();
			out.ints.clear();
			out.floats.clear();
			out.vec2s.clear();
			out.vec3s.clear();
		}

		for (const auto& s : overrides.textures) {
			bool replaced = false;
			for (auto& d : out.textures) {
				if (d.uniformName == s.uniformName) {
					d.textureHandle = s.textureHandle;
					d.unit = s.unit;
					replaced = true;
					break;
				}
			}
			if (!replaced) out.textures.push_back(s);
		}

		overrideByName(out.ints, overrides.ints);
		overrideByName(out.floats, overrides.floats);
		overrideByName(out.vec2s, overrides.vec2s);
		overrideByName(out.vec3s, overrides.vec3s);
	}

	void MaterialInstance::setInt(const std::string& name, int value) {
		upsert(overrides.ints, name, value);
	}

	void MaterialInstance::setFloat(const std::string& name, float value) {
		upsert(overrides.floats, name, value);
	}

	void MaterialInstance::setVec2(const std::string& name, const glm::vec2& value) {
		upsert(overrides.vec2s, name, value);
	}

}

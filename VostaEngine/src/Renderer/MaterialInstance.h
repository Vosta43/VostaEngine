#pragma once

#include "Material.h"

#include <string>

namespace ve {

	// A per-object view of a shared Material: the same shader and the same
	// declared bindings, with named overrides layered on top. Runtime-only (like
	// Unreal's MID) — never serialized, and it knows nothing about what the
	// override names mean, so any kind of Material can have one.
	class VE_API MaterialInstance : public Material {
	public:

		static constexpr const char* kTypeTag = "material_instance";

		// The material being viewed. Null renders nothing useful.
		Ref<Material> base;

		// Overrides keyed by uniform name. A name the base already emits replaces
		// that entry in place; an unknown name is appended (harmless — a uniform
		// the shader does not declare has no location).
		MaterialBindingSet overrides;

		Ref<Shader> getShader() override { return base ? base->getShader() : nullptr; }

		// Runs the base first (it clears the set and declares every uniform it
		// needs, defaults included), then applies this instance's overrides.
		void fillBindings(MaterialBindingSet& out) const override;

		const char* typeTag() const override { return kTypeTag; }

		// Runtime-only: the on-disk material is the base's.
		void serialize(Archive&) const override {}
		void deserialize(Archive&) override {}

		void setTexture(const std::string& name, AssetHandle texture, uint32_t unit);
		void setInt(const std::string& name, int value);
		void setFloat(const std::string& name, float value);
		void setVec2(const std::string& name, const glm::vec2& value);

		// Drop every override but keep the base, so the instance can be rebuilt in
		// place instead of reallocated when the underlying data changes.
		void clearOverrides() { overrides = MaterialBindingSet{}; }
	};

}

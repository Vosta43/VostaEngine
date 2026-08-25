#pragma once

#include "Scene/EntityRegistry.h"

#include <typeindex>
#include <vector>
#include <functional>

namespace ve {

	// One "Add Component" library entry. `add` default-constructs the component
	// on the entity; `present` reports whether the entity already has it (so the
	// UI can hide it). Both are type-erased at registration time.
	struct ComponentTypeInfo {
		std::type_index type;                  // identity, used for dedup
		const char* displayName;
		const char* category;                  // for grouping, e.g. "Rendering"
		std::function<void(EntityRegistry&, Entity)> add;
		std::function<bool(EntityRegistry&, Entity)> present;
	};

	// Meyers singleton. A function-local static is safe from static-initialization-
	// order issues: VECOMPONENT's registration objects run before main() and
	// always find this already-constructed.
	inline std::vector<ComponentTypeInfo>& componentRegistry() {
		static std::vector<ComponentTypeInfo> reg;
		return reg;
	}

	// Called by each VECOMPONENT registration object. Components.h is included
	// from many translation units and each emits its own internal-linkage
	// registration object, so dedup by type keeps exactly one entry per component.
	inline void registerComponentType(const ComponentTypeInfo& info) {
		auto& reg = componentRegistry();
		for (const auto& existing : reg)
			if (existing.type == info.type)
				return;
		reg.push_back(info);
	}

} // namespace ve

// Registers `className` as an "Add Component" menu entry. Pairs with the
// existing VESTRUCT(className) that precedes the struct body. MUST appear AFTER
// the struct body, because the factory instantiates
// EntityRegistry::emplace<className>, which requires a complete type.
#define VECOMPONENT(className, displayName, category) \
	static struct __compReg_##className { \
		__compReg_##className() { \
			ve::registerComponentType({ \
				std::type_index(typeid(className)), \
				displayName, category, \
				[](ve::EntityRegistry& reg, ve::Entity e) { reg.emplace<className>(e); }, \
				[](ve::EntityRegistry& reg, ve::Entity e) { return reg.has<className>(e); } \
			}); \
		} \
	} __compRegInstance_##className;

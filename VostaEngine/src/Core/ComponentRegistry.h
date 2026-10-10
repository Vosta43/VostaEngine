#pragma once

#include "Core.h"
#include "Scene/EntityRegistry.h"
#include "Core/Json.h"

#include <string>
#include <typeindex>
#include <vector>
#include <utility>
#include <functional>

namespace ve {

	// One "Add Component" library entry. `add` default-constructs the component
	// on the entity; `present` reports whether the entity already has it (so the
	// UI can hide it). Both are type-erased at registration time.
	//
	// `initialize` is an optional post-step run once the component exists and is
	// populated — after `add` and after `deserialize`. It is where a component
	// acquires the resources it needs to be usable (e.g. a terrain's default
	// mesh and material), so every creation path shares one hook.
	struct ComponentTypeInfo {
		std::type_index type;                  // identity, used for dedup
		const char* displayName;
		const char* category;                  // for grouping, e.g. "Rendering"
		const char* typeKey;                   // stable serialization key (class name)
		std::function<void(EntityRegistry&, Entity)> add;
		std::function<bool(EntityRegistry&, Entity)> present;
		std::function<void(EntityRegistry&, Entity, JsonWriter&)> serialize;
		std::function<void(EntityRegistry&, Entity, const JsonReader&)> deserialize;
		std::function<void(EntityRegistry&, Entity)> initialize;
	};

	// The one component registry, owned by the engine DLL (see
	// ComponentRegistry.cpp). Declared VE_API so a game module imports this single
	// instance instead of compiling its own copy -- two copies would mean the
	// engine never sees components a module registers.
	VE_API std::vector<ComponentTypeInfo>& componentRegistry();

	// Called by each VECOMPONENT registration object. Components.h is included
	// from many translation units and each emits its own internal-linkage
	// registration object, so dedup by type keeps exactly one entry per component.
	// Returns true only for the registration that actually inserted the entry.
	inline bool registerComponentType(const ComponentTypeInfo& info) {
		auto& reg = componentRegistry();
		for (const auto& existing : reg)
			if (existing.type == info.type)
				return false;
		reg.push_back(info);
		return true;
	}

	// Drops a type's entry. Called from VECOMPONENT's registration-object
	// destructor so a module's entries -- which hold module-code std::functions --
	// leave the registry before the module's DLL is freed.
	inline void unregisterComponentType(std::type_index type) {
		auto& reg = componentRegistry();
		for (auto it = reg.begin(); it != reg.end(); ++it) {
			if (it->type == type) {
				reg.erase(it);
				return;
			}
		}
	}

	inline const ComponentTypeInfo* findComponentType(const std::string& typeKey) {
		for (const auto& info : componentRegistry())
			if (typeKey == info.typeKey)
				return &info;
		return nullptr;
	}

	inline const ComponentTypeInfo* findComponentType(std::type_index type) {
		for (const auto& info : componentRegistry())
			if (type == info.type)
				return &info;
		return nullptr;
	}

	// Attach the post-add initializer for a component type. Call it from the
	// header that VECOMPONENT-declares the type, AFTER the registration object,
	// so the entry already exists (dynamic init is ordered within a TU).
	//
	// First registrant wins -- symmetric with registerComponentType. A game module
	// re-includes Components.h, so its TUs re-run this with a lambda compiled into
	// the module DLL; letting it overwrite would leave the registry holding a
	// function pointer that dangles once the module is unloaded.
	template<typename T>
	inline void setComponentInitializer(std::function<void(EntityRegistry&, Entity)> fn) {
		if (const ComponentTypeInfo* info = findComponentType(std::type_index(typeid(T)))) {
			auto* mutableInfo = const_cast<ComponentTypeInfo*>(info);
			if (!mutableInfo->initialize)
				mutableInfo->initialize = std::move(fn);
		}
	}

	// Run the init hook registered for T, if any.
	template<typename T>
	inline void runComponentInit(EntityRegistry& reg, Entity e) {
		if (const ComponentTypeInfo* info = findComponentType(std::type_index(typeid(T))))
			if (info->initialize)
				info->initialize(reg, e);
	}

} // namespace ve

// Registers `className` as an "Add Component" menu entry. Pairs with the
// existing VESTRUCT(className) that precedes the struct body. MUST appear AFTER
// the struct body, because the factory instantiates
// EntityRegistry::emplace<className>, which requires a complete type.
#define VECOMPONENT(className, displayName, category) \
	static struct __compReg_##className { \
		bool m_owned = false; \
		__compReg_##className() { \
			m_owned = ve::registerComponentType({ \
				std::type_index(typeid(className)), \
				displayName, category, \
				#className, \
				[](ve::EntityRegistry& reg, ve::Entity e) { reg.emplace<className>(e); ve::runComponentInit<className>(reg, e); }, \
				[](ve::EntityRegistry& reg, ve::Entity e) { return reg.has<className>(e); }, \
				[](ve::EntityRegistry& reg, ve::Entity e, ve::JsonWriter& w) { reg.get<className>(e).serialize(w); }, \
				[](ve::EntityRegistry& reg, ve::Entity e, const ve::JsonReader& r) { reg.emplace<className>(e).deserialize(r); ve::runComponentInit<className>(reg, e); } \
			}); \
		} \
		~__compReg_##className() { \
			if (m_owned) \
				ve::unregisterComponentType(std::type_index(typeid(className))); \
		} \
	} __compRegInstance_##className;

// Registers the post-add initializer for `className`. Pass the init body as a
// braced block; it runs with `reg` (EntityRegistry&) and `e` (Entity) in scope.
// Pairs with VECOMPONENT(className,...) and MUST appear AFTER it, so the
// registry entry exists when the initializer is attached.
//
//   VECOMPONENTINIT(TerrainComponent, {
//       ve::Terrain::ensureBuilt(reg.get<TerrainComponent>(e), e.m_id);
//   })
#define VECOMPONENTINIT(className, ...) \
	static struct __compInit_##className { \
		__compInit_##className() { \
			ve::setComponentInitializer<className>( \
				[](ve::EntityRegistry& reg, ve::Entity e) __VA_ARGS__); \
		} \
	} __compInitInstance_##className;

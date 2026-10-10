#include "vepch.h"
#include "ComponentRegistry.h"

namespace ve {

	// Owned here so exactly one instance exists across the engine DLL and every
	// loaded module. Intentionally leaked: a module's registration objects run
	// their destructors (and thus unregister) during DLL teardown, so the registry
	// must outlive static destruction. One small allocation for the process.
	std::vector<ComponentTypeInfo>& componentRegistry() {
		static std::vector<ComponentTypeInfo>* reg = new std::vector<ComponentTypeInfo>();
		return *reg;
	}

} // namespace ve

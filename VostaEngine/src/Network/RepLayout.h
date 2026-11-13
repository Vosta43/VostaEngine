#pragma once

#include "Core/Reflection.h"
#include "Network/BitReader.h"
#include "Network/BitWriter.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ve {

	using RepWriteFn   = void (*)(BitWriter&, const void*);
	using RepReadFn    = void (*)(BitReader&, void*);
	using RepCompareFn = bool (*)(const void*, const void*);
	// Writes a blend of a and b into dst. Null means the field is discrete and
	// snaps to the newest value instead of being interpolated.
	using RepInterpFn  = void (*)(void* dst, const void* a, const void* b, float alpha);

	struct RepTypeOps {
		uint32_t size = 0;
		RepWriteFn write = nullptr;
		RepReadFn read = nullptr;
		RepCompareFn compare = nullptr;
		RepInterpFn interp = nullptr;
	};

	struct RepField {
		std::string fieldName;   // wire identity; sorted to assign mask bits
		std::string typeName;
		std::function<void*(void*)> getter;
		uint32_t size = 0;       // bytes copied into a baseline snapshot
		RepWriteFn write = nullptr;
		RepReadFn read = nullptr;
		RepCompareFn compare = nullptr;
		RepInterpFn interp = nullptr;
	};

	struct RepLayout {
		std::string className;
		uint32_t classId = 0;
		std::vector<RepField> fields;   // index == mask bit
	};

	class VE_API RepLayoutRegistry {
	public:
		static RepLayoutRegistry& get();

		// Stable across builds as long as the class name does not change.
		static uint32_t stableClassId(const std::string& className);

		// typeName must match the spelling inside VEPROPERTY, e.g. "ve::Vec3".
		static void registerType(const std::string& typeName, const RepTypeOps& ops);

		// Type ops by the same spelling, for consumers that only need to
		// serialize a value (RPC arguments). Null when the type is unsupported.
		const RepTypeOps* findType(const std::string& typeName) const;

		// Collects replicated properties from every registered type, sorts each
		// class by fieldName, assigns class ids and freezes. On failure clears
		// everything and leaves the registry unfrozen.
		bool buildFromReflection(std::vector<std::string>& errors);

		// reset() + buildFromReflection(): call again whenever a game module
		// has registered its replicated properties.
		bool rebuild(std::vector<std::string>& errors);

		// Unfreezes and drops all layouts. Call before a game module unloads:
		// the getters stored in a layout point into that module's code.
		void reset();

		bool isFrozen() const { return m_frozen; }
		size_t layoutCount() const { return m_layouts.size(); }

		const RepLayout* find(const std::string& className) const;
		const RepLayout* find(uint32_t classId) const;

	private:
		RepLayoutRegistry() = default;

		std::unordered_map<std::string, RepTypeOps> m_types;
		std::unordered_map<std::string, RepLayout> m_layouts;
		std::unordered_map<uint32_t, const RepLayout*> m_byId;
		bool m_frozen = false;
	};

}

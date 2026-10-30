#pragma once

#include "Core/Core.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ve {

	constexpr uint16_t kInvalidComponentOffset = 0xFFFF;

	struct AttributeDesc {
		std::string name;
		uint16_t offset = 0;
		uint8_t comps = 1;
	};

	// Append-only: an existing name keeps its offset forever, new names go last.
	class VE_API AttributeRegistry {
	public:
		uint16_t getOrAdd(const std::string& name, uint8_t comps);

		uint16_t totalComponentsCount() const { return m_totalComponents; }
		uint32_t slotCount() const { return static_cast<uint32_t>(m_attrs.size()); }

		const AttributeDesc* get(uint16_t slot) const;
		const AttributeDesc* find(const std::string& name) const;

		// Registry slot of a name; kInvalidComponentOffset when missing. Diagnostics only.
		uint16_t findSlot(const std::string& name) const;

	private:
		std::vector<AttributeDesc> m_attrs;
		std::unordered_map<std::string, uint16_t> m_nameToSlot;
		uint16_t m_totalComponents = 0;
	};

}

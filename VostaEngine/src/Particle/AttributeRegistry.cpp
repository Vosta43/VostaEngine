#include "vepch.h"
#include "AttributeRegistry.h"

#include "Core/Log.h"

namespace ve {

	uint16_t AttributeRegistry::getOrAdd(const std::string& name, uint8_t comps) {
		if (comps == 0 || comps > 4) {
			VE_CORE_ERROR_PRINT("AttributeRegistry: '%s' width %u is outside 1..4",
				name.c_str(), static_cast<uint32_t>(comps));
			return kInvalidComponentOffset;
		}

		const auto it = m_nameToSlot.find(name);
		if (it != m_nameToSlot.end()) {
			const AttributeDesc& desc = m_attrs[it->second];
			if (desc.comps != comps) {
				VE_CORE_ERROR_PRINT("AttributeRegistry: '%s' registered as width %u, requested %u",
					name.c_str(), static_cast<uint32_t>(desc.comps), static_cast<uint32_t>(comps));
				return kInvalidComponentOffset;
			}
			return desc.offset;
		}

		const uint16_t offset = m_totalComponents;
		m_attrs.push_back({ name, offset, comps });
		m_nameToSlot.emplace(name, static_cast<uint16_t>(m_attrs.size() - 1));
		m_totalComponents = static_cast<uint16_t>(m_totalComponents + comps);
		return offset;
	}

	const AttributeDesc* AttributeRegistry::get(uint16_t slot) const {
		if (slot >= m_attrs.size())
			return nullptr;
		return &m_attrs[slot];
	}

	const AttributeDesc* AttributeRegistry::find(const std::string& name) const {
		const auto it = m_nameToSlot.find(name);
		if (it == m_nameToSlot.end())
			return nullptr;
		return &m_attrs[it->second];
	}

	uint16_t AttributeRegistry::findSlot(const std::string& name) const {
		const auto it = m_nameToSlot.find(name);
		return it == m_nameToSlot.end() ? kInvalidComponentOffset : it->second;
	}

}

#pragma once
#include <cstdint>

namespace ve {
	
	class VE_API AssetHandle {
	public:
		constexpr AssetHandle()
			: m_index(UINT32_MAX), m_generation(0) {
		}

		constexpr AssetHandle(uint32_t index, uint32_t generation)
			: m_index(index), m_generation(generation) {
		}
		
		constexpr bool isValid() const { return m_index != UINT32_MAX; }

		bool operator==(const AssetHandle& other) const {
			return m_index == other.m_index && m_generation == other.m_generation;
		}

		bool operator!=(const AssetHandle& other) const {
			return !(*this == other);
		}

		uint32_t index() const { return m_index; }
		
		uint32_t generation() const { return m_generation; }

	private:
		uint32_t m_index;
		uint32_t m_generation;
	};

	inline constexpr AssetHandle INVALID_ASSET_HANDLE = AssetHandle{ 0xFFFFFFFF, 0 };
}
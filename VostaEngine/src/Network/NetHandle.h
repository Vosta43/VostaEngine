#pragma once

#include <cstddef>
#include <cstdint>

namespace ve {

	// One replicated unit: a component instance on an entity.
	// classId picks the field layout, netId picks the entity.
	struct NetHandle {
		uint32_t netId = 0;
		uint32_t classId = 0;

		bool operator==(const NetHandle&) const = default;
	};

	struct NetHandleHash {
		size_t operator()(const NetHandle& h) const noexcept {
			return (size_t(h.netId) * 0x9E3779B97F4A7C15ull) ^ size_t(h.classId);
		}
	};

}

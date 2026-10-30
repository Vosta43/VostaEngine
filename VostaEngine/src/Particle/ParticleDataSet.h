#pragma once

#include "Core/Core.h"

#include <cstdint>
#include <vector>

namespace ve {

	// Component-major SoA: component c of instance i lives at data[c * capacity + i].
	struct VE_API ParticleDataSet {
		std::vector<float> data;
		uint32_t stride = 0;     // components per instance
		uint32_t num = 0;
		uint32_t capacity = 0;

		float* attr(uint16_t comp, uint32_t instance) {
			return data.data() + static_cast<size_t>(comp) * capacity + instance;
		}
		const float* attr(uint16_t comp, uint32_t instance) const {
			return data.data() + static_cast<size_t>(comp) * capacity + instance;
		}

		void ensureCapacity(uint32_t needed);
		void zeroInstance(uint32_t instance);
		void zeroAll();
		void copyInstance(const ParticleDataSet& src, uint32_t srcIndex, uint32_t dstIndex);
	};

}

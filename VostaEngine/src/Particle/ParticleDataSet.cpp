#include "vepch.h"
#include "ParticleDataSet.h"

namespace ve {

	namespace {
		constexpr uint32_t kMinCapacity = 1024;
	}

	void ParticleDataSet::ensureCapacity(uint32_t needed) {
		if (needed <= capacity)
			return;

		uint32_t newCapacity = std::max(capacity + capacity / 2, needed);
		newCapacity = std::max(newCapacity, kMinCapacity);

		std::vector<float> next(static_cast<size_t>(newCapacity) * stride, 0.0f);
		for (uint32_t c = 0; c < stride; ++c) {
			for (uint32_t i = 0; i < num; ++i) {
				next[static_cast<size_t>(c) * newCapacity + i] =
					data[static_cast<size_t>(c) * capacity + i];
			}
		}

		data.swap(next);
		capacity = newCapacity;
	}

	void ParticleDataSet::zeroInstance(uint32_t instance) {
		if (instance >= capacity)
			return;

		for (uint32_t c = 0; c < stride; ++c)
			data[static_cast<size_t>(c) * capacity + instance] = 0.0f;
	}

	void ParticleDataSet::zeroAll() {
		std::fill(data.begin(), data.end(), 0.0f);
	}

	void ParticleDataSet::copyInstance(const ParticleDataSet& src, uint32_t srcIndex, uint32_t dstIndex) {
		if (src.stride != stride)
			return;
		if (srcIndex >= src.capacity || dstIndex >= capacity)
			return;

		for (uint32_t c = 0; c < stride; ++c) {
			data[static_cast<size_t>(c) * capacity + dstIndex] =
				src.data[static_cast<size_t>(c) * src.capacity + srcIndex];
		}
	}

}

#pragma once

#include "Renderer/GpuReadback.h"

namespace ve {

	// Pixel-pack-buffer implementation of GpuReadback: two buffers, each with the
	// fence that marks the copy issued into it, so a fetch can be polled without
	// ever blocking on the GPU.
	class OpenGLGpuReadback : public GpuReadback {
	public:
		explicit OpenGLGpuReadback(size_t maxBytes);
		~OpenGLGpuReadback() override;

		bool request(const Ref<Texture>& texture, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
			uint32_t mip = 0) override;
		bool tryFetch(void* dst, size_t bytes) override;

	private:
		void release();

		static constexpr int kSlots = 2;

		uint32_t m_pbo[kSlots]    = { 0, 0 };
		void*    m_fence[kSlots]  = { nullptr, nullptr };
		uint64_t m_issued[kSlots] = { 0, 0 };
		uint64_t m_issueCount     = 0;
		size_t   m_maxBytes       = 0;
	};

}

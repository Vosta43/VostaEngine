#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"

#include <cstddef>
#include <cstdint>

namespace ve {

	// Reads texture texels back to the CPU without stalling the frame: a copy is
	// issued with a fence behind it, and the map happens on a later call, once that
	// fence has signalled. Two buffers alternate, so what is fetched is the oldest
	// request rather than the one being issued in the same frame. The result is a
	// GPU->CPU round trip that is always at least one frame behind the frame that
	// produced the data.
	class VE_API GpuReadback {
	public:
		virtual ~GpuReadback() = default;

		// Issues a copy of the `w`x`h` texels at (x, y) of `texture`'s level `mip`.
		// The call only enqueues the copy; it never waits on the GPU. Ordered after
		// any shader writes to that texture issued before this call. Returns false
		// -- issuing nothing -- while both buffers still have a request in flight.
		virtual bool request(const Ref<Texture>& texture, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
			uint32_t mip = 0) = 0;

		// Copies the oldest completed request into `dst` and returns true. False
		// means it is still in flight, or that there is nothing outstanding.
		virtual bool tryFetch(void* dst, size_t bytes) = 0;

		static Ref<GpuReadback> create(size_t maxBytes);
	};

}

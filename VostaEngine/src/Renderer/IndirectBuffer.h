#pragma once

#include "Core/Core.h"
#include "Renderer/Buffer.h"

namespace ve {

	// GPU-side storage for draw commands, consumed by
	// RendererAPI::drawIndexedIndirect / multiDrawIndexedIndirect. It has no
	// binding point of its own: the backend binds it to the indirect target for
	// the duration of the draw, so the same buffer can feed several passes.
	class VE_API IndirectBuffer {
	public:
		IndirectBuffer() {}
		virtual ~IndirectBuffer() = default;

		virtual void setData(const void* data, uint32_t byteSize, uint32_t byteOffset = 0) = 0;

		virtual uint32_t getSize() const = 0;
		virtual uint32_t getRendererID() const = 0;

		// Creates storage this object owns.
		static Ref<IndirectBuffer> create(uint32_t byteSize, BufferUsage usage);
		// Wraps a buffer someone else owns (e.g. a vertex buffer filled with
		// DrawArraysIndirectCommand records) so it can be passed to the indirect
		// draws without a copy. The wrapper never deletes the GL name.
		static Ref<IndirectBuffer> wrap(uint32_t rendererID, uint32_t byteSize);
	};

}

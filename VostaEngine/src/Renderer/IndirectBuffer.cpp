#include "vepch.h"
#include "IndirectBuffer.h"
#include "Platform/OpenGL/OpenGLIndirectBuffer.h"

namespace ve {

	Ref<IndirectBuffer> IndirectBuffer::create(uint32_t byteSize, BufferUsage usage) {
		return CreateRef<OpenGLIndirectBuffer>(byteSize, usage);
	}

	Ref<IndirectBuffer> IndirectBuffer::wrap(uint32_t rendererID, uint32_t byteSize) {
		return CreateRef<OpenGLIndirectBuffer>(rendererID, byteSize, false);
	}

}

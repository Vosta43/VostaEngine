#include "vepch.h"
#include "UniformBuffer.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace ve {

	Ref<UniformBuffer> UniformBuffer::create(uint32_t byteSize, uint32_t bindingPoint) {
		return CreateRef<OpenGLUniformBuffer>(byteSize, bindingPoint);
	}

}

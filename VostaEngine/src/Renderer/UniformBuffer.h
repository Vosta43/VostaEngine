#pragma once

#include "Core/Core.h"

namespace ve {

	// A std140 uniform block held on the GPU. The binding point is fixed at
	// creation: bind() rebinds this buffer to it, and a shader reaches the block
	// with Shader::bindUniformBlock(blockName, thatPoint). One upload per frame
	// replaces one uniform call per field.
	class VE_API UniformBuffer {
	public:
		UniformBuffer() {}
		virtual ~UniformBuffer() = default;

		virtual void bind() const = 0;
		virtual void unbind() const = 0;
		virtual void setData(const void* data, uint32_t byteSize, uint32_t byteOffset = 0) = 0;

		virtual uint32_t getSize() const = 0;
		virtual uint32_t getBindingPoint() const = 0;
		virtual uint32_t getRendererID() const = 0;

		static Ref<UniformBuffer> create(uint32_t byteSize, uint32_t bindingPoint);
	};

}

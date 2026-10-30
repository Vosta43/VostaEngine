#pragma once

#include "Renderer/UniformBuffer.h"

namespace ve {

	class OpenGLUniformBuffer : public UniformBuffer {
	public:
		OpenGLUniformBuffer(uint32_t byteSize, uint32_t bindingPoint);
		~OpenGLUniformBuffer();

		void bind() const override;
		void unbind() const override;
		void setData(const void* data, uint32_t byteSize, uint32_t byteOffset = 0) override;

		uint32_t getSize() const override { return m_size; }
		uint32_t getBindingPoint() const override { return m_bindingPoint; }
		uint32_t getRendererID() const override { return m_rendererID; }

	private:
		uint32_t m_rendererID = 0;
		uint32_t m_bindingPoint = 0;
		uint32_t m_size = 0;
	};

}

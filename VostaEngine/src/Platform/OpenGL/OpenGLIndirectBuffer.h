#pragma once

#include "Renderer/IndirectBuffer.h"

namespace ve {

	class OpenGLIndirectBuffer : public IndirectBuffer {
	public:
		OpenGLIndirectBuffer(uint32_t byteSize, BufferUsage usage);
		OpenGLIndirectBuffer(uint32_t rendererID, uint32_t byteSize, bool ownsBuffer);
		~OpenGLIndirectBuffer();

		void setData(const void* data, uint32_t byteSize, uint32_t byteOffset = 0) override;

		uint32_t getSize() const override { return m_size; }
		uint32_t getRendererID() const override { return m_rendererID; }

	private:
		uint32_t m_rendererID = 0;
		uint32_t m_size = 0;
		bool m_ownsBuffer = true;
	};

}

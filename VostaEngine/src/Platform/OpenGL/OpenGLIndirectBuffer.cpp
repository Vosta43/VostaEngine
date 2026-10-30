#include "vepch.h"
#include "OpenGLIndirectBuffer.h"
#include "Core/Log.h"

#include <glad/glad.h>

namespace ve {

	static GLenum toGLBufferUsage(BufferUsage usage) {
		switch (usage) {
		case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
		case BufferUsage::Stream:  return GL_STREAM_DRAW;
		case BufferUsage::Static:
		default:                   return GL_STATIC_DRAW;
		}
	}

	OpenGLIndirectBuffer::OpenGLIndirectBuffer(uint32_t byteSize, BufferUsage usage)
		: m_size(byteSize) {
		// Created target-less: the indirect target is bound only for the draw, so
		// there is no state to leave bound here.
		glCreateBuffers(1, &m_rendererID);
		glNamedBufferData(m_rendererID, byteSize, nullptr, toGLBufferUsage(usage));
	}

	OpenGLIndirectBuffer::OpenGLIndirectBuffer(uint32_t rendererID, uint32_t byteSize, bool ownsBuffer)
		: m_rendererID(rendererID), m_size(byteSize), m_ownsBuffer(ownsBuffer) {
	}

	OpenGLIndirectBuffer::~OpenGLIndirectBuffer() {
		if (m_ownsBuffer && m_rendererID) {
			glDeleteBuffers(1, &m_rendererID);
			m_rendererID = 0;
		}
	}

	void OpenGLIndirectBuffer::setData(const void* data, uint32_t byteSize, uint32_t byteOffset) {
		if (static_cast<size_t>(byteOffset) + byteSize > m_size) {
			VE_CORE_ERROR_PRINT("OpenGLIndirectBuffer::setData out of bounds (%u + %u > %u)",
			                    byteOffset, byteSize, m_size);
			return;
		}
		glNamedBufferSubData(m_rendererID, byteOffset, byteSize, data);
	}

}

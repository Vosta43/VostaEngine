#include "vepch.h"
#include "OpenGLUniformBuffer.h"
#include "Core/Log.h"

#include <glad/glad.h>

namespace ve {

	OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t byteSize, uint32_t bindingPoint)
		: m_bindingPoint(bindingPoint), m_size(byteSize) {
		glCreateBuffers(1, &m_rendererID);
		glNamedBufferData(m_rendererID, byteSize, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGLUniformBuffer::~OpenGLUniformBuffer() {
		if (m_rendererID) {
			// Leaving a deleted buffer bound to the point would make the next
			// draw read freed storage.
			glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingPoint, 0);
			glDeleteBuffers(1, &m_rendererID);
			m_rendererID = 0;
		}
	}

	void OpenGLUniformBuffer::bind() const {
		glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingPoint, m_rendererID);
	}

	void OpenGLUniformBuffer::unbind() const {
		glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingPoint, 0);
	}

	void OpenGLUniformBuffer::setData(const void* data, uint32_t byteSize, uint32_t byteOffset) {
		if (static_cast<size_t>(byteOffset) + byteSize > m_size) {
			VE_CORE_ERROR_PRINT("OpenGLUniformBuffer::setData out of bounds (%u + %u > %u)",
			                    byteOffset, byteSize, m_size);
			return;
		}
		glNamedBufferSubData(m_rendererID, byteOffset, byteSize, data);
	}

}

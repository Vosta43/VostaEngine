#include "vepch.h"
#include "OpenGLBuffer.h"

#include <glad/glad.h>

namespace ve {

    OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size)
        : m_count(size / sizeof(float)) {
        glCreateBuffers(1, &m_rendererId);
        glNamedBufferData(m_rendererId, size, vertices, GL_STATIC_DRAW);
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer() {
        glDeleteBuffers(1, &m_rendererId);
    }

    void OpenGLVertexBuffer::bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
    }

    void OpenGLVertexBuffer::unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void OpenGLVertexBuffer::setData(const void* data, uint32_t size)
    {
        glNamedBufferSubData(m_rendererId, 0, size, data);
    }


    OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count)
        : m_count(count) {
        glCreateBuffers(1, &m_rendererId);
        glNamedBufferData(m_rendererId, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer() {
        glDeleteBuffers(1, &m_rendererId);
    }

    void OpenGLIndexBuffer::bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererId);
    }

    void OpenGLIndexBuffer::unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

}
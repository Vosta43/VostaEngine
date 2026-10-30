#include "vepch.h"
#include "OpenGLBuffer.h"
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

    OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size)
        : m_count(size / sizeof(float)), m_size(size) {
        glCreateBuffers(1, &m_rendererId);
        glNamedBufferData(m_rendererId, size, vertices, GL_STATIC_DRAW);
    }

    OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t byteSize, BufferUsage usage)
        : m_count(0), m_size(byteSize) {
        glCreateBuffers(1, &m_rendererId);
        glNamedBufferData(m_rendererId, byteSize, nullptr, toGLBufferUsage(usage));
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

    void OpenGLVertexBuffer::setSubData(const void* data, uint32_t byteSize, uint32_t byteOffset)
    {
        if (static_cast<size_t>(byteOffset) + byteSize > m_size) {
            VE_CORE_ERROR_PRINT("OpenGLVertexBuffer::setSubData out of bounds (%u + %u > %u)",
                                byteOffset, byteSize, m_size);
            return;
        }
        glNamedBufferSubData(m_rendererId, byteOffset, byteSize, data);
    }

    void OpenGLVertexBuffer::copyFrom(const VertexBuffer& src, uint32_t dstByteOffset,
                                      uint32_t srcByteOffset, uint32_t byteSize)
    {
        glCopyNamedBufferSubData(src.getRendererID(), m_rendererId,
                                 srcByteOffset, dstByteOffset, byteSize);
    }

    OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count)
        : m_count(count), m_size(count * sizeof(uint32_t)) {
        glCreateBuffers(1, &m_rendererId);
        glNamedBufferData(m_rendererId, m_size, indices, GL_STATIC_DRAW);
    }

    OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t indexCapacity, BufferUsage usage)
        : m_count(0), m_size(indexCapacity * sizeof(uint32_t)) {
        glCreateBuffers(1, &m_rendererId);
        glNamedBufferData(m_rendererId, m_size, nullptr, toGLBufferUsage(usage));
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

    void OpenGLIndexBuffer::setSubData(const uint32_t* indices, uint32_t indexCount, uint32_t firstIndex)
    {
        const size_t byteOffset = static_cast<size_t>(firstIndex) * sizeof(uint32_t);
        const size_t byteSize = static_cast<size_t>(indexCount) * sizeof(uint32_t);
        if (byteOffset + byteSize > m_size) {
            VE_CORE_ERROR_PRINT("OpenGLIndexBuffer::setSubData out of bounds (%u + %u > %u indices)",
                                firstIndex, indexCount, m_size / sizeof(uint32_t));
            return;
        }
        glNamedBufferSubData(m_rendererId, byteOffset, byteSize, indices);
    }

    void OpenGLIndexBuffer::copyFrom(const IndexBuffer& src, uint32_t dstIndexOffset,
                                     uint32_t srcIndexOffset, uint32_t indexCount)
    {
        const GLsizeiptr byteSize = static_cast<GLsizeiptr>(indexCount) * sizeof(uint32_t);
        glCopyNamedBufferSubData(src.getRendererID(), m_rendererId,
                                 static_cast<GLintptr>(srcIndexOffset) * sizeof(uint32_t),
                                 static_cast<GLintptr>(dstIndexOffset) * sizeof(uint32_t),
                                 byteSize);
    }

}

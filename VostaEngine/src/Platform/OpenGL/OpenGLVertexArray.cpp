#include "vepch.h"
#include "OpenGLVertexArray.h"
#include "Renderer/Buffer.h"
#include <glad/glad.h>

namespace ve {
    
    static uint32_t shaderDataTypeToGLType(ShaderDataType type) {
        switch (type) {
        case ShaderDataType::Float:
        case ShaderDataType::Float2:
        case ShaderDataType::Float3:
        case ShaderDataType::Float4:
            return GL_FLOAT;
        case ShaderDataType::Int:
        case ShaderDataType::Int2:
        case ShaderDataType::Int3:
        case ShaderDataType::Int4:
            return GL_INT;
        case ShaderDataType::None:
            break;
        }
        return 0;
    }

    OpenGLVertexArray::OpenGLVertexArray() {
        glCreateVertexArrays(1, &m_rendererId);
    }

    OpenGLVertexArray::~OpenGLVertexArray() {
        glDeleteVertexArrays(1, &m_rendererId);
    }

    void OpenGLVertexArray::bind() const {
        glBindVertexArray(m_rendererId);
    }

    void OpenGLVertexArray::unbind() const {
        glBindVertexArray(0);
    }

    void OpenGLVertexArray::addVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) {
        glBindVertexArray(m_rendererId);
        vertexBuffer->bind();

        const auto& layout = vertexBuffer->getLayout();
        for (const auto& element : layout) {
            glEnableVertexAttribArray(m_vertexBufferIndex);
            glVertexAttribPointer(m_vertexBufferIndex,
                getShaderDataTypeComponentCount(element.shaderDataType),
                shaderDataTypeToGLType(element.shaderDataType),
                element.normalized ? GL_TRUE : GL_FALSE,
                layout.getStride(),
                (const void*)(uintptr_t)element.offset);
            m_vertexBufferIndex++;
        }

        m_vertexBuffers.push_back(vertexBuffer);
    }

    void OpenGLVertexArray::setIndexBuffer(const Ref<IndexBuffer>& indexBuffer) {
        glBindVertexArray(m_rendererId);
        indexBuffer->bind();
        m_indexBuffer = indexBuffer;
    }

    Ref<VertexArray> VertexArray::create() {
        return CreateRef<OpenGLVertexArray>();
    }

}
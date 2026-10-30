#include "vepch.h"
#include "OpenGLVertexArray.h"
#include "Renderer/Buffer.h"
#include "Core/Log.h"
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
        case ShaderDataType::Uint:
        case ShaderDataType::Uint2:
        case ShaderDataType::Uint3:
        case ShaderDataType::Uint4:
            return GL_UNSIGNED_INT;
        case ShaderDataType::Int8:
        case ShaderDataType::Int8x2:
        case ShaderDataType::Int8x3:
        case ShaderDataType::Int8x4:
            return GL_BYTE;
        case ShaderDataType::Uint8:
        case ShaderDataType::Uint8x2:
        case ShaderDataType::Uint8x3:
        case ShaderDataType::Uint8x4:
            return GL_UNSIGNED_BYTE;
        case ShaderDataType::Uint16:
        case ShaderDataType::Uint16x2:
        case ShaderDataType::Uint16x3:
        case ShaderDataType::Uint16x4:
            return GL_UNSIGNED_SHORT;
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
            // An explicit location wins; otherwise the attribute takes the next
            // index free across every buffer added to this vertex array.
            const uint32_t location = (element.location != BufferElement::AutoLocation)
                ? element.location : m_vertexBufferIndex;
            const GLenum glType = shaderDataTypeToGLType(element.shaderDataType);
            glEnableVertexAttribArray(location);
            if (element.integer && glType != GL_FLOAT) {
                // Integer attributes bypass the float conversion and must be
                // declared as ivec/uvec in the shader.
                glVertexAttribIPointer(location,
                    getShaderDataTypeComponentCount(element.shaderDataType),
                    glType,
                    layout.getStride(),
                    (const void*)(uintptr_t)element.offset);
            } else {
                if (element.integer)
                    VE_CORE_ERROR_PRINT("Vertex attribute '%s' is marked integer but has a float type; "
                                        "using the converted path", element.name.c_str());
                glVertexAttribPointer(location,
                    getShaderDataTypeComponentCount(element.shaderDataType),
                    glType,
                    element.normalized ? GL_TRUE : GL_FALSE,
                    layout.getStride(),
                    (const void*)(uintptr_t)element.offset);
            }
            glVertexAttribDivisor(location, element.instanced ? 1 : 0);
            if (location >= m_vertexBufferIndex)
                m_vertexBufferIndex = location + 1;
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
#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "Core/Core.h"

namespace ve {
    
    enum class ShaderDataType {
        None = 0,
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4
    };

    static uint32_t getShaderDataTypeSize(ShaderDataType type) {
        switch (type) {
        case ShaderDataType::Float:  return 4;
        case ShaderDataType::Float2: return 4 * 2;
        case ShaderDataType::Float3: return 4 * 3;
        case ShaderDataType::Float4: return 4 * 4;
        case ShaderDataType::Int:    return 4;
        case ShaderDataType::Int2:   return 4 * 2;
        case ShaderDataType::Int3:   return 4 * 3;
        case ShaderDataType::Int4:   return 4 * 4;
        case ShaderDataType::None:   return 0;
        }
        return 0;
    }

    static uint32_t getShaderDataTypeComponentCount(ShaderDataType type) {
        switch (type) {
        case ShaderDataType::Float:  return 1;
        case ShaderDataType::Float2: return 2;
        case ShaderDataType::Float3: return 3;
        case ShaderDataType::Float4: return 4;
        case ShaderDataType::Int:    return 1;
        case ShaderDataType::Int2:   return 2;
        case ShaderDataType::Int3:   return 3;
        case ShaderDataType::Int4:   return 4;
        case ShaderDataType::None:   return 0;
        }
        return 0;
    }

    struct BufferElement {
        std::string name;
        ShaderDataType shaderDataType;
        uint32_t size;
        uint32_t offset;
        bool normalized;

        BufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
            : name(name)
            , shaderDataType(type)
            , size(getShaderDataTypeSize(type))
            , offset(0)
            , normalized(normalized) {
        }
    };

    class BufferLayout {
    public:
        BufferLayout() = default;
        BufferLayout(const std::initializer_list<BufferElement>& elements)
            : m_bufferElements(elements) {
            calculateOffsetsAndStride();
        }

        inline const std::vector<BufferElement>& getBufferElements() const { return m_bufferElements; }
        inline uint32_t getStride() const { return m_stride; }

        auto begin() { return m_bufferElements.begin(); }
        auto end() { return m_bufferElements.end(); }
        auto begin() const { return m_bufferElements.begin(); }
        auto end() const { return m_bufferElements.end(); }

    private:
        void calculateOffsetsAndStride() {
            uint32_t offset = 0;
            m_stride = 0;
            for (auto& element : m_bufferElements) {
                element.offset = offset;
                offset += element.size;
                m_stride += element.size;
            }
        }

        std::vector<BufferElement> m_bufferElements;
        uint32_t m_stride = 0;
    };

    class VE_API VertexBuffer {
    public:
        virtual ~VertexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual uint32_t getCount() const = 0;
        virtual const BufferLayout& getLayout() const = 0;
        virtual void setLayout(const BufferLayout& layout) = 0;
        virtual void setData(const void* data, uint32_t size) = 0;
        static Ref<VertexBuffer> create(float* vertices, uint32_t size);
    };

    class VE_API IndexBuffer {
    public:
        virtual ~IndexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual uint32_t getCount() const = 0;

        static Ref<IndexBuffer> create(uint32_t* indices, uint32_t count);
    };



}
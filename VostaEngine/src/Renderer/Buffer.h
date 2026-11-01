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
        Int4,
        // Narrow integer components, for packed vertex structs. A normalized one
        // reaches the shader as a float; an `integer` element reaches it as an
        // ivec/uvec and must be declared that way. Appended so the earlier values
        // keep their meaning.
        Int8,
        Int8x2,
        Int8x3,
        Int8x4,
        Uint8,
        Uint8x2,
        Uint8x3,
        Uint8x4,
        Uint16,
        Uint16x2,
        Uint16x3,
        Uint16x4,
        Uint,
        Uint2,
        Uint3,
        Uint4
    };

    // Bytes per component (1, 2 or 4). Also the attribute's natural alignment.
    static uint32_t getShaderDataTypeComponentSize(ShaderDataType type) {
        switch (type) {
        case ShaderDataType::Int8:
        case ShaderDataType::Int8x2:
        case ShaderDataType::Int8x3:
        case ShaderDataType::Int8x4:
        case ShaderDataType::Uint8:
        case ShaderDataType::Uint8x2:
        case ShaderDataType::Uint8x3:
        case ShaderDataType::Uint8x4:
            return 1;
        case ShaderDataType::Uint16:
        case ShaderDataType::Uint16x2:
        case ShaderDataType::Uint16x3:
        case ShaderDataType::Uint16x4:
            return 2;
        case ShaderDataType::Float:
        case ShaderDataType::Float2:
        case ShaderDataType::Float3:
        case ShaderDataType::Float4:
        case ShaderDataType::Int:
        case ShaderDataType::Int2:
        case ShaderDataType::Int3:
        case ShaderDataType::Int4:
        case ShaderDataType::Uint:
        case ShaderDataType::Uint2:
        case ShaderDataType::Uint3:
        case ShaderDataType::Uint4:
            return 4;
        case ShaderDataType::None:
            return 0;
        }
        return 0;
    }

    static uint32_t getShaderDataTypeComponentCount(ShaderDataType type) {
        switch (type) {
        case ShaderDataType::Float:
        case ShaderDataType::Int:
        case ShaderDataType::Int8:
        case ShaderDataType::Uint8:
        case ShaderDataType::Uint16:
        case ShaderDataType::Uint:
            return 1;
        case ShaderDataType::Float2:
        case ShaderDataType::Int2:
        case ShaderDataType::Int8x2:
        case ShaderDataType::Uint8x2:
        case ShaderDataType::Uint16x2:
        case ShaderDataType::Uint2:
            return 2;
        case ShaderDataType::Float3:
        case ShaderDataType::Int3:
        case ShaderDataType::Int8x3:
        case ShaderDataType::Uint8x3:
        case ShaderDataType::Uint16x3:
        case ShaderDataType::Uint3:
            return 3;
        case ShaderDataType::Float4:
        case ShaderDataType::Int4:
        case ShaderDataType::Int8x4:
        case ShaderDataType::Uint8x4:
        case ShaderDataType::Uint16x4:
        case ShaderDataType::Uint4:
            return 4;
        case ShaderDataType::None:
            return 0;
        }
        return 0;
    }

    static uint32_t getShaderDataTypeSize(ShaderDataType type) {
        return getShaderDataTypeComponentSize(type) * getShaderDataTypeComponentCount(type);
    }

    struct BufferElement {
        // Sentinel: the element takes the next free attribute location.
        static constexpr uint32_t AutoLocation = 0xFFFFFFFFu;

        std::string name;
        ShaderDataType shaderDataType;
        uint32_t size;
        uint32_t offset;
        bool normalized;
        // Advanced once per instance instead of per vertex (attribute divisor 1).
        bool instanced;
        // Raw integer attribute: the shader input is an int/uint vector and the
        // bits are passed through unconverted. Only meaningful for Int*/Uint*.
        bool integer;
        // Shader input location (the `layout(location = N)` index), or
        // AutoLocation to take the next one in declaration order.
        uint32_t location;

        BufferElement(ShaderDataType type, const std::string& name, bool normalized = false,
                      bool instanced = false, bool integer = false)
            : name(name)
            , shaderDataType(type)
            , size(getShaderDataTypeSize(type))
            , offset(0)
            , normalized(normalized)
            , instanced(instanced)
            , integer(integer)
            , location(AutoLocation) {
        }

        // Explicit location, for layouts whose attribute numbering is not the
        // declaration order (a skipped index, or a shared index across buffers).
        BufferElement(ShaderDataType type, uint32_t location, const std::string& name,
                      bool normalized = false, bool instanced = false, bool integer = false)
            : name(name)
            , shaderDataType(type)
            , size(getShaderDataTypeSize(type))
            , offset(0)
            , normalized(normalized)
            , instanced(instanced)
            , integer(integer)
            , location(location) {
        }
    };

    class BufferLayout {
    public:
        BufferLayout() = default;
        BufferLayout(const std::initializer_list<BufferElement>& elements)
            : m_bufferElements(elements) {
            calculateOffsetsAndStride(0);
        }
        // Overrides the computed stride for structs whose padding does not follow
        // component alignment (e.g. a trailing uint32 after narrow attributes).
        BufferLayout(const std::initializer_list<BufferElement>& elements, uint32_t explicitStride)
            : m_bufferElements(elements) {
            calculateOffsetsAndStride(explicitStride);
        }
        // Keeps the caller's own element offsets (a packed struct laid out by
        // offsetof) instead of recomputing them, and adopts `explicitStride`.
        // Built at runtime, so the initializer_list forms cannot express it.
        BufferLayout(const std::vector<BufferElement>& elements, uint32_t explicitStride)
            : m_bufferElements(elements)
            , m_stride(explicitStride) {
        }

        inline const std::vector<BufferElement>& getBufferElements() const { return m_bufferElements; }
        inline uint32_t getStride() const { return m_stride; }

        auto begin() { return m_bufferElements.begin(); }
        auto end() { return m_bufferElements.end(); }
        auto begin() const { return m_bufferElements.begin(); }
        auto end() const { return m_bufferElements.end(); }

    private:
        // Offsets follow natural alignment of each component, so a layout mirrors
        // the C struct it describes. Stride is the padded size, or `explicitStride`.
        void calculateOffsetsAndStride(uint32_t explicitStride) {
            uint32_t offset = 0;
            uint32_t maxAlign = 1;
            for (auto& element : m_bufferElements) {
                const uint32_t align = getShaderDataTypeComponentSize(element.shaderDataType);
                if (align > maxAlign) maxAlign = align;
                if (align > 1)
                    offset = (offset + align - 1) / align * align;
                element.offset = offset;
                offset += element.size;
            }
            if (explicitStride > 0)
                m_stride = explicitStride;
            else
                m_stride = maxAlign > 1 ? (offset + maxAlign - 1) / maxAlign * maxAlign : offset;
        }

        std::vector<BufferElement> m_bufferElements;
        uint32_t m_stride = 0;
    };

    // Storage hint for a buffer the GPU keeps resident. Stream is for data
    // rewritten most frames (command lists, per-frame vertex streams), Dynamic
    // for occasional rewrites, Static for write-once geometry.
    enum class BufferUsage : uint8_t {
        Static,
        Dynamic,
        Stream
    };

    class VE_API VertexBuffer {
    public:
        virtual ~VertexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual uint32_t getCount() const = 0;
        // Capacity in bytes.
        virtual uint32_t getSize() const = 0;
        // The backend's buffer id; copyFrom() uses it as the GPU-side source.
        virtual uint32_t getRendererID() const = 0;
        virtual const BufferLayout& getLayout() const = 0;
        virtual void setLayout(const BufferLayout& layout) = 0;
        virtual void setData(const void* data, uint32_t size) = 0;
        // Resizes the storage while keeping the buffer's identity, so VAOs that
        // already reference it stay valid. Contents are undefined afterwards.
        virtual void reallocate(uint32_t byteSize) = 0;
        // Writes a byte range, leaving the rest of the buffer untouched. A
        // remeshed chunk lands in its own slot of a shared buffer through this
        // instead of re-uploading everything before it.
        virtual void setSubData(const void* data, uint32_t byteSize, uint32_t byteOffset) = 0;
        // GPU-side copy; used to compact a shared buffer's live prefix before it
        // is reallocated.
        virtual void copyFrom(const VertexBuffer& src, uint32_t dstByteOffset,
                              uint32_t srcByteOffset, uint32_t byteSize) = 0;

        static Ref<VertexBuffer> create(float* vertices, uint32_t size);
        // Allocates `byteSize` bytes of undefined contents.
        static Ref<VertexBuffer> create(uint32_t byteSize, BufferUsage usage);
    };

    class VE_API IndexBuffer {
    public:
        virtual ~IndexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual uint32_t getCount() const = 0;
        // Capacity in bytes.
        virtual uint32_t getSize() const = 0;
        virtual uint32_t getRendererID() const = 0;
        // drawIndexed() draws getCount() indices, while an indirect command
        // carries its own count, so a buffer whose live prefix moves must set
        // this explicitly.
        virtual void setCount(uint32_t indexCount) = 0;
        virtual void setSubData(const uint32_t* indices, uint32_t indexCount, uint32_t firstIndex) = 0;
        virtual void copyFrom(const IndexBuffer& src, uint32_t dstIndexOffset,
                              uint32_t srcIndexOffset, uint32_t indexCount) = 0;

        static Ref<IndexBuffer> create(uint32_t* indices, uint32_t count);
        // Allocates `indexCapacity` indices (4 bytes each) of undefined contents.
        static Ref<IndexBuffer> create(uint32_t indexCapacity, BufferUsage usage);
    };



}
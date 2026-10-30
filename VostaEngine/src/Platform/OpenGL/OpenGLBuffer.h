#pragma once

#include "Renderer/Buffer.h"

namespace ve {
    class OpenGLVertexBuffer : public VertexBuffer {
    public:
        OpenGLVertexBuffer(float* vertices, uint32_t size);
        OpenGLVertexBuffer(uint32_t byteSize, BufferUsage usage);
        virtual ~OpenGLVertexBuffer();

        virtual void bind() const override;
        virtual void unbind() const override;

        virtual uint32_t getCount() const override { return m_count; }
        virtual uint32_t getSize() const override { return m_size; }
        virtual uint32_t getRendererID() const override { return m_rendererId; }
        virtual const BufferLayout& getLayout() const override { return m_layout; }
        virtual void setLayout(const BufferLayout& layout) override { m_layout = layout; }
        virtual void setData(const void* data, uint32_t size) override;
        virtual void setSubData(const void* data, uint32_t byteSize, uint32_t byteOffset) override;
        virtual void copyFrom(const VertexBuffer& src, uint32_t dstByteOffset,
                              uint32_t srcByteOffset, uint32_t byteSize) override;

    private:
        uint32_t m_rendererId;
        uint32_t m_count;
        uint32_t m_size = 0;
        BufferLayout m_layout;
    };


    class OpenGLIndexBuffer : public IndexBuffer {
    public:
        OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
        OpenGLIndexBuffer(uint32_t indexCapacity, BufferUsage usage);
        virtual ~OpenGLIndexBuffer();

        virtual void bind() const override;
        virtual void unbind() const override;

        virtual uint32_t getCount() const override { return m_count; }
        virtual uint32_t getSize() const override { return m_size; }
        virtual uint32_t getRendererID() const override { return m_rendererId; }
        virtual void setCount(uint32_t indexCount) override { m_count = indexCount; }
        virtual void setSubData(const uint32_t* indices, uint32_t indexCount, uint32_t firstIndex) override;
        virtual void copyFrom(const IndexBuffer& src, uint32_t dstIndexOffset,
                              uint32_t srcIndexOffset, uint32_t indexCount) override;

    private:
        uint32_t m_rendererId;
        uint32_t m_count;
        uint32_t m_size = 0;
    };

}

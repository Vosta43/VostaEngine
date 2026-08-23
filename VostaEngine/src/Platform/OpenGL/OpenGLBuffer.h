#pragma once

#include "Renderer/Buffer.h"

namespace ve {
    class OpenGLVertexBuffer : public VertexBuffer {
    public:
        OpenGLVertexBuffer(float* vertices, uint32_t size);
        virtual ~OpenGLVertexBuffer();

        virtual void bind() const override;
        virtual void unbind() const override;

        virtual uint32_t getCount() const override { return m_count; }
        virtual const BufferLayout& getLayout() const override { return m_layout; }
        virtual void setLayout(const BufferLayout& layout) override { m_layout = layout; }
        virtual void setData(const void* data, uint32_t size);
        //virtual uint32_t getRendererID() const override { return m_rendererId; }

    private:
        uint32_t m_rendererId;
        uint32_t m_count;
        BufferLayout m_layout;
    };


    class OpenGLIndexBuffer : public IndexBuffer {
    public:
        OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
        virtual ~OpenGLIndexBuffer();

        virtual void bind() const override;
        virtual void unbind() const override;

        virtual uint32_t getCount() const override { return m_count; }

    private:
        uint32_t m_rendererId;
        uint32_t m_count;
    };

}
#include "vepch.h"
#include "Buffer.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

namespace ve {

    Ref<VertexBuffer> VertexBuffer::create(float* vertices, uint32_t size) {
        return CreateRef<OpenGLVertexBuffer>(vertices, size);
    }
    Ref<IndexBuffer> IndexBuffer::create(uint32_t* indices, uint32_t count) {
        return CreateRef<OpenGLIndexBuffer>(indices, count);
    }

}

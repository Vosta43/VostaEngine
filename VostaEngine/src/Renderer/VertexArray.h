#pragma once

#include <memory>
#include <cstdint>
#include <vector>

#include "Core/Core.h"

namespace ve {

    class VertexBuffer;
    class IndexBuffer;

    class VE_API VertexArray {
    public:
        virtual ~VertexArray() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual void addVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) = 0;
        virtual void setIndexBuffer(const Ref<IndexBuffer>& indexBuffer) = 0;

        virtual const Ref<IndexBuffer>& getIndexBuffer() const = 0;

        static Ref<VertexArray> create();
    };


}
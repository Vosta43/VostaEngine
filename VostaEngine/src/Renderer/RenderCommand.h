#pragma once

#include <memory>
#include <cstdint>

#include "Core/Core.h"
#include "Renderer/RendererAPI.h"

namespace ve {

    class VertexArray;
    class RendererAPI;
    class IndirectBuffer;

    class VE_API RenderCommand {
    public:
        static void init(bool reverseZ = true);
        static void setClearColor(float r, float g, float b, float a);
        static void clear();
        static void clear(bool color, bool depth);
        // Clear the bound framebuffer's integer color attachment to `value`.
        static void clearInt(int32_t value);
        static void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        static void setScissor(int32_t x, int32_t y, int32_t width, int32_t height);
        static void setScissorTest(bool enabled);
        static void drawIndexed(const Ref<VertexArray>& vertexArray);
        // Draw a sub-range of the bound index buffer instead of the whole thing.
        static void drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount,
                                uint32_t firstIndex = 0, int32_t baseVertex = 0);
        static void drawIndexedInstanced(const Ref<VertexArray>& vertexArray, uint32_t instanceCount,
                                         uint32_t indexCount = 0, uint32_t firstIndex = 0,
                                         int32_t baseVertex = 0);
        static void drawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                        const Ref<IndirectBuffer>& commands, uint32_t byteOffset = 0);
        static void multiDrawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                             const Ref<IndirectBuffer>& commands,
                                             uint32_t byteOffset, uint32_t drawCount,
                                             uint32_t byteStride = 0);

        static void drawArrays(uint32_t count, uint32_t firstVertex = 0);
        static void drawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount,
                                        uint32_t firstVertex = 0);
        static void drawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                       const Ref<IndirectBuffer>& commands, uint32_t byteOffset = 0);
        static void multiDrawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                            const Ref<IndirectBuffer>& commands,
                                            uint32_t byteOffset, uint32_t drawCount,
                                            uint32_t byteStride = 0);
        // Drops the backend's cached GL state after outside code touched it.
        static void invalidateStateCache();
        // Declarative state: the backend applies only what differs from what it
        // already bound. The per-field setters below take the same path with one
        // field changed, so the two can never disagree.
        static void applyPipelineState(const RenderStateDesc& state);
        static void setDepthFunc(CompareOp func);

        static void setDepthMask(bool enabled);

        static void setDepthTesting(bool enabled);
        static void setBlend(bool enabled);
        static void setBlendFunc(BlendFactor src, BlendFactor dst);
        static void setBlendAttachmentMask(uint8_t attachmentMask);
        static void setColorWriteMask(uint8_t attachmentMask);
        static void setWireframe(bool enabled);
        static void setPolygonOffset(bool enabled, float factor, float units);
        static void setDepthRange(float nearDepth01, float farDepth01);
        static void setLineWidth(float widthInPixels);
        static void setPointSize(float sizeInPixels);
        static void setUnpackAlignment(uint32_t alignment);
        static void setPackAlignment(uint32_t alignment);

    private:
        static RendererAPI* s_RendererAPI;
    };

}

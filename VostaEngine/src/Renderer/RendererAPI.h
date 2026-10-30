#pragma once

#include <glm.hpp>
#include <memory>
#include <cstdint>

#include "Core/Core.h"
#include "Renderer/PipelineState.h"

namespace ve {

    class VertexArray;
    class IndirectBuffer;

    // RendererAPI defines the platform-agnostic rendering interface.
    // It is inherited by platform-specific implementations (e.g., OpenGLRendererAPI) and invoked exclusively through the RenderCommand layer.
    class RendererAPI {
    public:
        enum class API {
            None = 0,
            OpenGL = 1,
            Vulkan = 2,
            DirectX = 3,
            Metal = 4
        };

    public:
        virtual ~RendererAPI() = default;

        // reverseZ: engine 0..1 clip space (depth 0 = far) vs standard GL depth.
        virtual void init(bool reverseZ = true) = 0;
        virtual void setClearColor(const glm::vec4& color) = 0;
        virtual void clear() = 0;
        // Clear only the requested attachments, so a pass can wipe colour while
        // keeping the depth it just wrote (or the reverse).
        virtual void clear(bool color, bool depth) = 0;
        // Clear the color attachment as a signed integer (for integer buffers,
        // e.g. the R32I picking target), then clear depth.
        virtual void clearInt(int32_t value) = 0;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        // Scissor rectangle in pixels (origin bottom-left); only applied while scissor test is enabled.
        virtual void setScissor(int32_t x, int32_t y, int32_t width, int32_t height) = 0;
        virtual void setScissorTest(bool enabled) = 0;
        // Submit an indexed draw call. The VertexArray must be bound and its index buffer populated before calling.
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray) = 0;
        // Submit a sub-range: indexCount indices starting at firstIndex (an offset into the index buffer),
        // added to baseVertex so several chunks can share one vertex buffer.
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount,
                                 uint32_t firstIndex = 0, int32_t baseVertex = 0) = 0;
        // Submit the index buffer once per instance. Attributes declared instanced
        // in the VertexArray's layout advance once per instance. indexCount 0 draws
        // the whole buffer; firstIndex and baseVertex mean what they do for
        // drawIndexed.
        virtual void drawIndexedInstanced(const Ref<VertexArray>& vertexArray, uint32_t instanceCount,
                                          uint32_t indexCount = 0, uint32_t firstIndex = 0,
                                          int32_t baseVertex = 0) = 0;
        // Non-indexed draws: instance the bound vertex arrays (particles), or let
        // the GPU read DrawArraysIndirectCommand records from a shared buffer.
        virtual void drawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount,
                                         uint32_t firstVertex = 0) = 0;
        virtual void drawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                        const Ref<IndirectBuffer>& commands,
                                        uint32_t byteOffset = 0) = 0;
        virtual void multiDrawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                             const Ref<IndirectBuffer>& commands,
                                             uint32_t byteOffset, uint32_t drawCount,
                                             uint32_t byteStride = 0) = 0;
        // One DrawElementsIndirectCommand at byteOffset: the GPU reads the index
        // count and offsets, so one shared vertex/index buffer can feed many
        // draws whose ranges are decided on the CPU each frame.
        virtual void drawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                         const Ref<IndirectBuffer>& commands,
                                         uint32_t byteOffset = 0) = 0;
        // `drawCount` commands starting at `byteOffset`, `byteStride` bytes apart
        // (0 = tightly packed).
        virtual void multiDrawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                              const Ref<IndirectBuffer>& commands,
                                              uint32_t byteOffset, uint32_t drawCount,
                                              uint32_t byteStride = 0) = 0;
        virtual void setDepthTesting(bool enabled) = 0;

        virtual void drawArrays(uint32_t count, uint32_t firstVertex = 0) = 0;
        // Applies a whole render state. The backend keeps what it has already
        // bound and issues only the difference, so callers state what they want
        // rather than tracking what is currently set.
        virtual void applyPipelineState(const RenderStateDesc& state) = 0;
        // Drops the backend's state mirror after something outside this interface
        // changed GL (third-party UI, legacy code mid-migration). The next
        // applyPipelineState re-sends every field instead of diffing.
        virtual void invalidateStateCache() = 0;
        virtual void setDepthFunc(CompareOp func) = 0;
        virtual void setDepthMask(bool enabled) = 0;
        virtual void setBlend(bool enabled) = 0;
        virtual void setBlendFunc(BlendFactor src, BlendFactor dst) = 0;
        // Per-attachment blend enable: bit i blends colour attachment i. 0xFF is
        // the plain global switch; anything else leaves the listed attachments
        // blended and the rest written unblended, which is how a translucent
        // overlay is kept out of a deferred pack's material attachments.
        virtual void setBlendAttachmentMask(uint8_t attachmentMask) = 0;
        // Per-attachment write mask: bit i writes colour attachment i. Clearing
        // a bit leaves that attachment's contents from the previous draw.
        virtual void setColorWriteMask(uint8_t attachmentMask) = 0;
        // Rasterize triangles as outlines (true) or filled (false); a debug view.
        virtual void setWireframe(bool enabled) = 0;
        // Depth bias in normalized depth units. Under this renderer's reverse-Z
        // depth the derived offset points the other way, so the same bias value
        // that pushes a surface toward the camera in normal clip space pulls it
        // away here.
        virtual void setPolygonOffset(bool enabled, float factor, float units) = 0;
        // Maps NDC depth into the 0..1 window range: [0,1] is the full range,
        // smaller spans clamp depth writes (an overlay confined to a slab).
        virtual void setDepthRange(float nearDepth01, float farDepth01) = 0;
        virtual void setLineWidth(float widthInPixels) = 0;
        virtual void setPointSize(float sizeInPixels) = 0;
        // Row alignment for client-side pixel transfers. Rows whose byte width is
        // not a multiple of the current alignment need 1, both when uploading
        // tightly packed blocks and when reading a rectangle back.
        virtual void setUnpackAlignment(uint32_t alignment) = 0;
        virtual void setPackAlignment(uint32_t alignment) = 0;

        static API getAPI() {return s_API;}
        static bool isReverseZ() { return s_reverseZ; }

        static RendererAPI* create();

    protected:
        // Written by the backend's init(reverseZ); read through isReverseZ().
        static inline bool s_reverseZ = true;

    private:
        // Stores the active graphics API backend. Used to branch API-specific code paths at runtime.
        static API s_API;
    };

}

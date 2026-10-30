#pragma once

#include "Renderer/RendererAPI.h"

namespace ve {

    class OpenGLRendererAPI : public RendererAPI {
    public:
        virtual void init(bool reverseZ = true) override;
        virtual void setClearColor(const glm::vec4& color) override;
        virtual void clear() override;
        void clear(bool color, bool depth) override;
        void clearInt(int32_t value) override;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        virtual void setScissor(int32_t x, int32_t y, int32_t width, int32_t height) override;
        virtual void setScissorTest(bool enabled) override;
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray) override;
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount,
                                 uint32_t firstIndex = 0, int32_t baseVertex = 0) override;
        virtual void drawIndexedInstanced(const Ref<VertexArray>& vertexArray, uint32_t instanceCount,
                                          uint32_t indexCount = 0, uint32_t firstIndex = 0,
                                          int32_t baseVertex = 0) override;
        void drawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount,
                                 uint32_t firstVertex = 0) override;
        void drawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                const Ref<IndirectBuffer>& commands,
                                uint32_t byteOffset = 0) override;
        void multiDrawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                     const Ref<IndirectBuffer>& commands,
                                     uint32_t byteOffset, uint32_t drawCount,
                                     uint32_t byteStride = 0) override;
        virtual void drawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                         const Ref<IndirectBuffer>& commands,
                                         uint32_t byteOffset = 0) override;
        virtual void multiDrawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                              const Ref<IndirectBuffer>& commands,
                                              uint32_t byteOffset, uint32_t drawCount,
                                              uint32_t byteStride = 0) override;
        void setDepthTesting(bool enabled) override;
        void drawArrays(uint32_t count, uint32_t firstVertex = 0) override;
        void applyPipelineState(const RenderStateDesc& state) override;
        void invalidateStateCache() override;
        void setDepthFunc(CompareOp func) override;
        void setDepthMask(bool enabled) override;
        void setBlend(bool enabled) override;
        void setBlendFunc(BlendFactor src, BlendFactor dst) override;
        void setBlendAttachmentMask(uint8_t attachmentMask) override;
        void setColorWriteMask(uint8_t attachmentMask) override;
        void setWireframe(bool enabled) override;
        void setPolygonOffset(bool enabled, float factor, float units) override;
        void setDepthRange(float nearDepth01, float farDepth01) override;
        void setLineWidth(float widthInPixels) override;
        void setPointSize(float sizeInPixels) override;
        void setUnpackAlignment(uint32_t alignment) override;
        void setPackAlignment(uint32_t alignment) override;

    private:
        // The single truth about GL state: every setter below mutates this and
        // then goes through applyPipelineState, so declarative and per-field
        // callers cannot diverge.
        RenderStateDesc m_current;
        // True once a per-attachment blend mask has disabled some index; the next
        // "all attachments" state must re-enable them explicitly.
        bool m_blendIndexedDirty = false;
        // Set by invalidateStateCache(): the mirror no longer describes GL, so the
        // next applyPipelineState re-sends every field instead of diffing.
        bool m_stateUnknown = false;
    };

}

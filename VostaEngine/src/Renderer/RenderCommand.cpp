#include "vepch.h"
#include "RenderCommand.h"
#include "Renderer/VertexArray.h"

namespace ve {

    RendererAPI* RenderCommand::s_RendererAPI = RendererAPI::create();

    void RenderCommand::init(bool reverseZ){
        s_RendererAPI->init(reverseZ);
    }

    void RenderCommand::bindDefaultFramebuffer() {
        s_RendererAPI->bindDefaultFramebuffer();
    }

    void RenderCommand::setClearColor(float r, float g, float b, float a) {
        s_RendererAPI->setClearColor(glm::vec4(r, g, b, a));
    }

    void RenderCommand::clear() {
        s_RendererAPI->clear();
    }

    void RenderCommand::clear(bool color, bool depth) {
        s_RendererAPI->clear(color, depth);
    }

    void RenderCommand::clearInt(int32_t value) {
        s_RendererAPI->clearInt(value);
    }

    void RenderCommand::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        s_RendererAPI->setViewport(x, y, width, height);
    }

    void RenderCommand::getViewport(int32_t out[4]) {
        s_RendererAPI->getViewport(out);
    }

    uint32_t RenderCommand::getBoundFramebuffer() {
        return s_RendererAPI->getBoundFramebuffer();
    }

    void RenderCommand::rendererInfo(std::string& renderer, std::string& vendor) {
        s_RendererAPI->rendererInfo(renderer, vendor);
    }

    void RenderCommand::setScissor(int32_t x, int32_t y, int32_t width, int32_t height) {
        s_RendererAPI->setScissor(x, y, width, height);
    }

    void RenderCommand::setScissorTest(bool enabled) {
        s_RendererAPI->setScissorTest(enabled);
    }

    void RenderCommand::drawIndexed(const Ref<VertexArray>& vertexArray) {
        s_RendererAPI->drawIndexed(vertexArray);
    }

    void RenderCommand::drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount,
                                    uint32_t firstIndex, int32_t baseVertex) {
        s_RendererAPI->drawIndexed(vertexArray, indexCount, firstIndex, baseVertex);
    }

    void RenderCommand::drawIndexedInstanced(const Ref<VertexArray>& vertexArray, uint32_t instanceCount,
                                             uint32_t indexCount, uint32_t firstIndex, int32_t baseVertex) {
        s_RendererAPI->drawIndexedInstanced(vertexArray, instanceCount, indexCount, firstIndex, baseVertex);
    }

    void RenderCommand::drawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                            const Ref<IndirectBuffer>& commands, uint32_t byteOffset) {
        s_RendererAPI->drawIndexedIndirect(vertexArray, commands, byteOffset);
    }

    void RenderCommand::multiDrawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                                 const Ref<IndirectBuffer>& commands,
                                                 uint32_t byteOffset, uint32_t drawCount,
                                                 uint32_t byteStride) {
        s_RendererAPI->multiDrawIndexedIndirect(vertexArray, commands, byteOffset, drawCount, byteStride);
    }

    void RenderCommand::setDepthTesting(bool enabled) {
        s_RendererAPI->setDepthTesting(enabled);
    }

    void RenderCommand::drawArrays(uint32_t count, uint32_t firstVertex) {
        s_RendererAPI->drawArrays(count, firstVertex);
    }

    void RenderCommand::drawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount,
                                            uint32_t firstVertex) {
        s_RendererAPI->drawArraysInstanced(vertexCount, instanceCount, firstVertex);
    }

    void RenderCommand::drawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                           const Ref<IndirectBuffer>& commands, uint32_t byteOffset) {
        s_RendererAPI->drawArraysIndirect(vertexArray, commands, byteOffset);
    }

    void RenderCommand::multiDrawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                                const Ref<IndirectBuffer>& commands,
                                                uint32_t byteOffset, uint32_t drawCount,
                                                uint32_t byteStride) {
        s_RendererAPI->multiDrawArraysIndirect(vertexArray, commands, byteOffset, drawCount, byteStride);
    }

    void RenderCommand::applyPipelineState(const RenderStateDesc& state) {
        s_RendererAPI->applyPipelineState(state);
    }

    void RenderCommand::invalidateStateCache() {
        s_RendererAPI->invalidateStateCache();
    }

    RenderStateDesc RenderCommand::currentState() {
        return s_RendererAPI->currentState();
    }

    void RenderCommand::setDepthFunc(CompareOp func) {
        s_RendererAPI->setDepthFunc(func);
    }

    void RenderCommand::setDepthMask(bool enabled) {
        s_RendererAPI->setDepthMask(enabled);
    }

    void RenderCommand::setBlend(bool enabled) {
        s_RendererAPI->setBlend(enabled);
    }

    void RenderCommand::setBlendFunc(BlendFactor src, BlendFactor dst) {
        s_RendererAPI->setBlendFunc(src, dst);
    }

    void RenderCommand::setBlendAttachmentMask(uint8_t attachmentMask) {
        s_RendererAPI->setBlendAttachmentMask(attachmentMask);
    }

    void RenderCommand::setColorWriteMask(uint8_t attachmentMask) {
        s_RendererAPI->setColorWriteMask(attachmentMask);
    }

    void RenderCommand::setWireframe(bool enabled) {
        s_RendererAPI->setWireframe(enabled);
    }

    void RenderCommand::setPolygonOffset(bool enabled, float factor, float units) {
        s_RendererAPI->setPolygonOffset(enabled, factor, units);
    }

    void RenderCommand::setDepthRange(float nearDepth01, float farDepth01) {
        s_RendererAPI->setDepthRange(nearDepth01, farDepth01);
    }

    void RenderCommand::setLineWidth(float widthInPixels) {
        s_RendererAPI->setLineWidth(widthInPixels);
    }

    void RenderCommand::setPointSize(float sizeInPixels) {
        s_RendererAPI->setPointSize(sizeInPixels);
    }

    void RenderCommand::setUnpackAlignment(uint32_t alignment) {
        s_RendererAPI->setUnpackAlignment(alignment);
    }

    void RenderCommand::setPackAlignment(uint32_t alignment) {
        s_RendererAPI->setPackAlignment(alignment);
    }

    void RenderCommand::readPixels(uint32_t x, uint32_t y, uint32_t w, uint32_t h, void* out) {
        s_RendererAPI->readPixels(x, y, w, h, out);
    }

}
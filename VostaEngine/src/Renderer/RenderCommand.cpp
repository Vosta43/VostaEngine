#include "vepch.h"
#include "RenderCommand.h"
#include "Renderer/VertexArray.h"

namespace ve {

    RendererAPI* RenderCommand::s_RendererAPI = RendererAPI::create();

    void RenderCommand::init(){
        s_RendererAPI->init();
    }

    void RenderCommand::setClearColor(float r, float g, float b, float a) {
        s_RendererAPI->setClearColor(glm::vec4(r, g, b, a));
    }

    void RenderCommand::clear() {
        s_RendererAPI->clear();
    }

    void RenderCommand::clearInt(int32_t value) {
        s_RendererAPI->clearInt(value);
    }

    void RenderCommand::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        s_RendererAPI->setViewport(x, y, width, height);
    }

    void RenderCommand::drawIndexed(const Ref<VertexArray>& vertexArray) {
        s_RendererAPI->drawIndexed(vertexArray);
    }

    void RenderCommand::setDepthTesting(bool enabled) {
        s_RendererAPI->setDepthTesting(enabled);
    }

    void RenderCommand::setCursorVisible(bool visible) {
        s_RendererAPI->setCursorVisible(visible);
    }

    void RenderCommand::drawArrays(uint32_t count) {
        s_RendererAPI->drawArrays(count);
    }

    void RenderCommand::setDepthFunc(RendererAPI::DepthFunc func) {
        s_RendererAPI->setDepthFunc(func);
    }

    void RenderCommand::setDepthMask(bool enabled) {
        s_RendererAPI->setDepthMask(enabled);
    }

    void RenderCommand::setBlend(bool enabled) {
        s_RendererAPI->setBlend(enabled);
    }

    void RenderCommand::setBlendFunc(RendererAPI::BlendFunc src, RendererAPI::BlendFunc dst) {
        s_RendererAPI->setBlendFunc(src, dst);
    }

    void RenderCommand::setWireframe(bool enabled) {
        s_RendererAPI->setWireframe(enabled);
    }

}
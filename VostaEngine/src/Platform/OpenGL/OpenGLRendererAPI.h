#pragma once

#include "Renderer/RendererAPI.h"

namespace ve {

    class OpenGLRendererAPI : public RendererAPI {
    public:
        virtual void init() override;
        virtual void setClearColor(const glm::vec4& color) override;
        virtual void clear() override;
        void clearInt(int32_t value) override;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        virtual void setScissor(int32_t x, int32_t y, int32_t width, int32_t height) override;
        virtual void setScissorTest(bool enabled) override;
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray) override;
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t firstIndex = 0) override;
        void setDepthTesting(bool enabled) override;
        // TODO: Decouple cursor visibility from the rendering API. This belongs to the windowing/input layer.
        void setCursorVisible(bool visible) override;
        void drawArrays(uint32_t count) override;
        void setDepthFunc(DepthFunc func) override;
        void setDepthMask(bool enabled) override;
        void setBlend(bool enabled) override;
        void setBlendFunc(BlendFunc src, BlendFunc dst) override;
        void setWireframe(bool enabled) override;
    };

}

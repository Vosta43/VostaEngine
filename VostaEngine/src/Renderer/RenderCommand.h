#pragma once

#include <memory>
#include <cstdint>

#include "Core/Core.h"
#include "Renderer/RendererAPI.h"

namespace ve {

    class VertexArray;
    class RendererAPI;

    class VE_API RenderCommand {
    public:
        static void init();
        static void setClearColor(float r, float g, float b, float a);
        static void clear();
        // Clear the bound framebuffer's integer color attachment to `value`.
        static void clearInt(int32_t value);
        static void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        static void setScissor(int32_t x, int32_t y, int32_t width, int32_t height);
        static void setScissorTest(bool enabled);
        static void drawIndexed(const Ref<VertexArray>& vertexArray);
        // Draw a sub-range of the bound index buffer instead of the whole thing.
        static void drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t firstIndex = 0);

        static void drawArrays(uint32_t count);
        static void setDepthFunc(RendererAPI::DepthFunc func);

        static void setDepthMask(bool enabled);

        static void setDepthTesting(bool enabled);
        static void setCursorVisible(bool visible);
        static void setBlend(bool enabled);
        static void setBlendFunc(RendererAPI::BlendFunc src, RendererAPI::BlendFunc dst);
        static void setWireframe(bool enabled);

    private:
        static RendererAPI* s_RendererAPI;
    };

}

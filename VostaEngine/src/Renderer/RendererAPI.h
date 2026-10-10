#pragma once

#include <glm.hpp>
#include <memory>
#include <cstdint>

#include "Core/Core.h"

namespace ve {

    class VertexArray;

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
        enum class DepthFunc {
            LESS,
            LEQUAL
        };
        enum class BlendFunc {
            Zero,
            One,
            SrcAlpha,
            OneMinusSrcAlpha
        };

    public:
        virtual ~RendererAPI() = default;

        virtual void init() = 0;
        virtual void setClearColor(const glm::vec4& color) = 0;
        virtual void clear() = 0;
        // Clear the color attachment as a signed integer (for integer buffers,
        // e.g. the R32I picking target), then clear depth.
        virtual void clearInt(int32_t value) = 0;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        // Scissor rectangle in pixels (origin bottom-left); only applied while scissor test is enabled.
        virtual void setScissor(int32_t x, int32_t y, int32_t width, int32_t height) = 0;
        virtual void setScissorTest(bool enabled) = 0;
        // Submit an indexed draw call. The VertexArray must be bound and its index buffer populated before calling.
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray) = 0;
        // Submit a sub-range: indexCount indices starting at firstIndex (an offset into the index buffer).
        virtual void drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t firstIndex = 0) = 0;
        virtual void setDepthTesting(bool enabled) = 0;
        // TODO: Decouple cursor visibility from the rendering API. This belongs to the windowing/input layer.
        virtual void setCursorVisible(bool visible) = 0;

        virtual void drawArrays(uint32_t count) = 0;
        virtual void setDepthFunc(DepthFunc func) = 0;
        virtual void setDepthMask(bool enabled) = 0;
        virtual void setBlend(bool enabled) = 0;
        virtual void setBlendFunc(BlendFunc src, BlendFunc dst) = 0;
        // Rasterize triangles as outlines (true) or filled (false); a debug view.
        virtual void setWireframe(bool enabled) = 0;

        static API getAPI() {return s_API;}

        static RendererAPI* create();

    private:
        // Stores the active graphics API backend. Used to branch API-specific code paths at runtime.
        static API s_API;
    };

}

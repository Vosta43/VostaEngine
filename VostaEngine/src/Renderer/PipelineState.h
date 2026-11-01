#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "Core/Core.h"

namespace ve {

    // Backend-agnostic render state. These describe only what every graphics API
    // agrees on; anything only one backend has (vertex input, attachment formats,
    // shader modules) belongs in that backend's own pipeline key.

    enum class BlendFactor : uint8_t {
        Zero, One,
        SrcAlpha, OneMinusSrcAlpha,
        DstAlpha, OneMinusDstAlpha,
        SrcColor, OneMinusSrcColor
    };

    enum class CompareOp : uint8_t {
        Never, Less, LEqual, Greater, GEqual, Equal, NotEqual, Always
    };

    enum class CullMode : uint8_t {
        None, Back, Front
    };

    enum class FrontFace : uint8_t {
        CCW, CW
    };

    enum class PolygonMode : uint8_t {
        Fill, Line, Point
    };

    enum class PrimitiveType : uint8_t {
        Triangles, Lines, LineStrip, TriangleStrip, TriangleFan, Points
    };

    struct BlendDesc {
        bool enable = false;
        BlendFactor src = BlendFactor::One;
        BlendFactor dst = BlendFactor::Zero;
        // Which colour attachments the blend reaches; bit i = attachment i, and
        // 0xFF is the plain global switch.
        uint8_t attachments = 0xFF;

        // Factors and attachments describe nothing while blending is off, so they
        // are not compared then.
        bool operator==(const BlendDesc& other) const {
            return enable == other.enable && (!enable ||
                   (src == other.src && dst == other.dst && attachments == other.attachments));
        }
    };

    struct DepthStencilDesc {
        bool test = true;
        bool write = true;
        // Reverse-Z: the depth buffer holds 1 at the near plane, so "nearer wins"
        // is Greater.
        CompareOp compare = CompareOp::Greater;

        bool operator==(const DepthStencilDesc&) const = default;
    };

    struct RasterDesc {
        // None by default: culling off is what this renderer did before culling
        // was expressible, so a default-constructed state changes no output.
        CullMode cull = CullMode::None;
        FrontFace frontFace = FrontFace::CCW;
        PolygonMode polygonMode = PolygonMode::Fill;
        bool depthBias = false;
        float depthBiasFactor = 0.0f;
        float depthBiasUnits = 0.0f;
        float lineWidth = 1.0f;
        float pointSize = 1.0f;

        bool operator==(const RasterDesc&) const = default;
    };

    struct RenderStateDesc {
        BlendDesc blend;
        DepthStencilDesc depth;
        RasterDesc raster;
        PrimitiveType topology = PrimitiveType::Triangles;
        // Which colour attachments are written; bit i = attachment i.
        uint8_t colorWriteMask = 0xFF;

        bool operator==(const RenderStateDesc&) const = default;
    };

    // Index into PipelineStateCache (1-based; 0 = invalid).
    struct PipelineHandle {
        uint32_t id = 0;
        bool valid() const { return id != 0; }
    };

    // Immutable render states, deduplicated by value. Binding one hands the whole
    // descriptor to the backend, which applies only the fields that changed; a
    // backend with real pipeline objects (Vulkan) creates and caches them from
    // the same descriptor plus its own key dimensions.
    class VE_API PipelineStateCache {
    public:
        static PipelineStateCache& get();

        // Same descriptor in, same handle out.
        PipelineHandle getOrCreate(const RenderStateDesc& desc);
        void bind(PipelineHandle handle);
        // The descriptor behind `handle`; a default state for an invalid handle.
        const RenderStateDesc& desc(PipelineHandle handle) const;

        size_t size() const { return m_states.size(); }

    private:
        PipelineStateCache() = default;

        std::vector<RenderStateDesc> m_states;
    };

}

#pragma once

#include "Core/Core.h"
#include "Core/Log.h"
#include "FrameBuffer.h"
#include "Renderer/RenderPass/RenderPassBase.h"
#include "RenderContext.h"
#include "Renderer/RenderPass/GBufferPass.h"
#include "Renderer/RenderPass/HDRBufferPass.h"
#include "Renderer/RenderPass/CloudPass.h"
#include "Renderer/RenderPass/CloudTAAPass.h"
#include "Renderer/RenderPass/PostBufferPass.h"
#include "Renderer/RenderPass/TAAPass.h"
#include "RenderCommand.h"

#include <vector>

namespace ve {

    // Identify each render pass type for configuration and factory dispatch.
    enum class PassType {
        GBuffer = 0,
        HDRLighting,
        Cloud,
        CloudTAA,   // temporal accumulation of the quarter-res cloud buffer
        TAA,
        PostProcess
    };

    // Describes which passes to enable and in what order.
    struct PipelineConfig {
        std::vector<PassType> passes = {
            PassType::GBuffer,
            PassType::Cloud,          // quarter-res volumetric clouds — must run before HDR, which composites
            PassType::CloudTAA,       // cloud temporal accumulation — between Cloud and HDR, which composites it
            PassType::HDRLighting,
            PassType::TAA,            // temporal accumulation on linear HDR — must run before tonemapping
            PassType::PostProcess
        };
    };

    // Creates pass instances by type. New pass types register here.
    class PassFactory {
    public:
        static Ref<RenderPassBase> create(PassType type) {
            switch (type) {
                case PassType::GBuffer:     return CreateRef<GBufferPass>();
                case PassType::HDRLighting: return CreateRef<HDRBufferPass>();
                case PassType::Cloud:       return CreateRef<CloudPass>();
                case PassType::CloudTAA:    return CreateRef<CloudTAAPass>();
                case PassType::TAA:         return CreateRef<TAAPass>();
                case PassType::PostProcess: return CreateRef<PostBufferPass>();
                default:                    return nullptr;
            }
        }
    };

    class VE_API RenderPipeline {
    public:
        void init(uint32_t width, uint32_t height);
        void init(uint32_t width, uint32_t height, const PipelineConfig& config);
        void render(RenderContext& renderContext);
        void shutdown();

        void bindScreenShader() const { m_screenShader->bind(); }
        void unbindScreenShader() const { m_screenShader->unbind(); }
        Ref<Shader> getScreenShader() { return m_screenShader; }
        void drawFullscreenQuad() const {
            m_fullscreenQuad->bind();
            RenderCommand::drawIndexed(m_fullscreenQuad);
            m_fullscreenQuad->unbind();
        }

        void resize(uint32_t width, uint32_t height);

    private:
        void createFramebuffers(uint32_t width, uint32_t height);
        void buildPasses();

        Ref<Framebuffer> m_GBuffer;
        Ref<Framebuffer> m_HDRBuffer;
        Ref<Framebuffer> m_cloudBuffer;
        Ref<Framebuffer> m_taaHistory[2];
        Ref<Framebuffer> m_cloudTaaHistory[2];
        Ref<Framebuffer> m_postBuffer;

        std::vector<Ref<RenderPassBase>> m_passes;
        PipelineConfig m_config;

        uint32_t m_width;
        uint32_t m_height;

        bool m_initialized = false;

        Ref<Shader> m_screenShader;
        Ref<VertexArray> m_fullscreenQuad;
    };

}
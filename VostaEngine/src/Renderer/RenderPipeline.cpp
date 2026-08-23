#include "vepch.h"
#include "RenderPipeline.h"
#include "Core/Application.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"

namespace ve {

    void RenderPipeline::init(uint32_t width, uint32_t height)
    {
        init(width, height, PipelineConfig{});
    }

    void RenderPipeline::init(uint32_t width, uint32_t height, const PipelineConfig& config)
    {
        m_width = width;
        m_height = height;
        m_config = config;
        m_initialized = true;

        createFramebuffers(width, height);
        buildPasses();

        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("SandBox/assets/shaders/screen.glsl");
        m_screenShader = shaderLib.get("screen");

        float vertices[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f,
            -1.0f,  1.0f
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        m_fullscreenQuad = VertexArray::create();
        auto vb = VertexBuffer::create(vertices, sizeof(vertices));
        BufferLayout layout = { { ShaderDataType::Float2, "a_Position" } };
        vb->setLayout(layout);
        m_fullscreenQuad->addVertexBuffer(vb);
        auto ib = IndexBuffer::create(indices, 6);
        m_fullscreenQuad->setIndexBuffer(ib);
    }

    void RenderPipeline::createFramebuffers(uint32_t width, uint32_t height)
    {
        for (auto passType : m_config.passes) {
            switch (passType) {
                case PassType::GBuffer: {
                    FramebufferSpec spec;
                    spec.width = width;
                    spec.height = height;
                    spec.hasDepthStencil = true;
                    spec.colorAttachments.resize(4);

                    spec.colorAttachments[0].internalFormat = TextureInternalFormat::RGBA8;
                    spec.colorAttachments[0].format = TextureDataFormat::RGBA;
                    spec.colorAttachments[0].type = TextureDataType::UNSIGNED_BYTE;
                    spec.colorAttachments[0].minFilter = TextureFilter::LINEAR;
                    spec.colorAttachments[0].magFilter = TextureFilter::LINEAR;

                    spec.colorAttachments[1].internalFormat = TextureInternalFormat::RGBA16F;
                    spec.colorAttachments[1].format = TextureDataFormat::RGBA;
                    spec.colorAttachments[1].type = TextureDataType::FLOAT;
                    spec.colorAttachments[1].minFilter = TextureFilter::LINEAR;
                    spec.colorAttachments[1].magFilter = TextureFilter::LINEAR;

                    spec.colorAttachments[2].internalFormat = TextureInternalFormat::RGBA8;
                    spec.colorAttachments[2].format = TextureDataFormat::RGBA;
                    spec.colorAttachments[2].type = TextureDataType::UNSIGNED_BYTE;
                    spec.colorAttachments[2].minFilter = TextureFilter::LINEAR;
                    spec.colorAttachments[2].magFilter = TextureFilter::LINEAR;

                    spec.colorAttachments[3].internalFormat = TextureInternalFormat::RGBA8;
                    spec.colorAttachments[3].format = TextureDataFormat::RGBA;
                    spec.colorAttachments[3].type = TextureDataType::UNSIGNED_BYTE;
                    spec.colorAttachments[3].minFilter = TextureFilter::LINEAR;
                    spec.colorAttachments[3].magFilter = TextureFilter::LINEAR;

                    m_GBuffer = Framebuffer::create(spec);
                    break;
                }
                case PassType::HDRLighting: {
                    FramebufferSpec spec;
                    spec.width = width;
                    spec.height = height;
                    spec.hasDepthStencil = false;
                    spec.colorAttachments.resize(1);

                    spec.colorAttachments[0].internalFormat = TextureInternalFormat::RGBA16F;
                    spec.colorAttachments[0].format = TextureDataFormat::RGBA;
                    spec.colorAttachments[0].type = TextureDataType::FLOAT;
                    spec.colorAttachments[0].minFilter = TextureFilter::LINEAR;
                    spec.colorAttachments[0].magFilter = TextureFilter::LINEAR;

                    m_HDRBuffer = Framebuffer::create(spec);
                    break;
                }
                case PassType::Cloud: {
                    FramebufferSpec spec;
                    spec.width  = std::max(1u, width / 4);
                    spec.height = std::max(1u, height / 4);
                    spec.hasDepthStencil = false;
                    spec.colorAttachments.resize(1);
                    spec.colorAttachments[0].internalFormat = TextureInternalFormat::RGBA16F;
                    spec.colorAttachments[0].format = TextureDataFormat::RGBA;
                    spec.colorAttachments[0].type = TextureDataType::FLOAT;
                    spec.colorAttachments[0].minFilter = TextureFilter::LINEAR;
                    spec.colorAttachments[0].magFilter = TextureFilter::LINEAR;

                    m_cloudBuffer = Framebuffer::create(spec);
                    break;
                }
                case PassType::CloudTAA: {
                    // Ping-pong pair for the accumulated cloud history. Same
                    // quarter-res size as the cloud buffer it averages, so
                    // sampling maps 1:1 and HDR composites the accumulated frame.
                    for (int i = 0; i < 2; i++) {
                        FramebufferSpec spec;
                        spec.width  = std::max(1u, width / 4);
                        spec.height = std::max(1u, height / 4);
                        spec.hasDepthStencil = false;
                        spec.colorAttachments.resize(1);
                        spec.colorAttachments[0].internalFormat = TextureInternalFormat::RGBA16F;
                        spec.colorAttachments[0].format = TextureDataFormat::RGBA;
                        spec.colorAttachments[0].type = TextureDataType::FLOAT;
                        spec.colorAttachments[0].minFilter = TextureFilter::LINEAR;
                        spec.colorAttachments[0].magFilter = TextureFilter::LINEAR;

                        m_cloudTaaHistory[i] = Framebuffer::create(spec);
                    }
                    break;
                }
                case PassType::TAA: {
                    // Ping-pong history pair. Full-res linear HDR, same format
                    // as the HDR buffer so the accumulated frame feeds straight
                    // into PostBufferPass's tonemap.
                    for (int i = 0; i < 2; i++) {
                        FramebufferSpec spec;
                        spec.width = width;
                        spec.height = height;
                        spec.hasDepthStencil = false;
                        spec.colorAttachments.resize(1);
                        spec.colorAttachments[0].internalFormat = TextureInternalFormat::RGBA16F;
                        spec.colorAttachments[0].format = TextureDataFormat::RGBA;
                        spec.colorAttachments[0].type = TextureDataType::FLOAT;
                        spec.colorAttachments[0].minFilter = TextureFilter::LINEAR;
                        spec.colorAttachments[0].magFilter = TextureFilter::LINEAR;

                        m_taaHistory[i] = Framebuffer::create(spec);
                    }
                    break;
                }
                case PassType::PostProcess: {
                    FramebufferSpec spec;
                    spec.width = width;
                    spec.height = height;
                    spec.hasDepthStencil = false;
                    spec.colorAttachments.resize(1);
                    spec.colorAttachments[0].internalFormat = TextureInternalFormat::RGBA8;
                    spec.colorAttachments[0].format = TextureDataFormat::RGBA;
                    spec.colorAttachments[0].type = TextureDataType::UNSIGNED_BYTE;
                    spec.colorAttachments[0].minFilter = TextureFilter::LINEAR;
                    spec.colorAttachments[0].magFilter = TextureFilter::LINEAR;

                    m_postBuffer = Framebuffer::create(spec);
                    break;
                }
            }
        }
    }

    void RenderPipeline::buildPasses()
    {
        m_passes.clear();

        for (auto passType : m_config.passes) {
            auto pass = PassFactory::create(passType);
            if (!pass) continue;

            // Attach the appropriate framebuffer to each pass.
            switch (passType) {
                case PassType::GBuffer:
                    static_cast<GBufferPass*>(pass.get())->setFramebuffer(m_GBuffer);
                    break;
                case PassType::HDRLighting:
                    static_cast<HDRBufferPass*>(pass.get())->setFramebuffer(m_HDRBuffer);
                    break;
                case PassType::Cloud:
                    static_cast<CloudPass*>(pass.get())->setFramebuffer(m_cloudBuffer);
                    break;
                case PassType::CloudTAA:
                    static_cast<CloudTAAPass*>(pass.get())->setHistoryBuffers(m_cloudTaaHistory[0], m_cloudTaaHistory[1]);
                    break;
                case PassType::TAA:
                    static_cast<TAAPass*>(pass.get())->setHistoryBuffers(m_taaHistory[0], m_taaHistory[1]);
                    break;
                case PassType::PostProcess:
                    static_cast<PostBufferPass*>(pass.get())->setFramebuffer(m_postBuffer);
                    break;
            }

            pass->init();
            m_passes.push_back(pass);
        }
    }

    void RenderPipeline::render(RenderContext& renderContext) {
        for (auto& pass : m_passes) {
            pass->execute(renderContext);
        }
    }

    void RenderPipeline::shutdown() {
        m_passes.clear();
        m_GBuffer.reset();
        m_HDRBuffer.reset();
        m_cloudBuffer.reset();
        m_taaHistory[0].reset();
        m_taaHistory[1].reset();
        m_cloudTaaHistory[0].reset();
        m_cloudTaaHistory[1].reset();
        m_postBuffer.reset();
        m_screenShader.reset();
        m_fullscreenQuad.reset();
        m_initialized = false;
    }

    void RenderPipeline::resize(uint32_t width, uint32_t height) {
        if (width == m_width && height == m_height) return;
        m_width = width;
        m_height = height;
        if (m_GBuffer) m_GBuffer->resize(width, height);
        if (m_HDRBuffer) m_HDRBuffer->resize(width, height);
        if (m_cloudBuffer) m_cloudBuffer->resize(std::max(1u, width / 4), std::max(1u, height / 4));
        if (m_taaHistory[0]) m_taaHistory[0]->resize(width, height);
        if (m_taaHistory[1]) m_taaHistory[1]->resize(width, height);
        if (m_cloudTaaHistory[0]) m_cloudTaaHistory[0]->resize(std::max(1u, width / 4), std::max(1u, height / 4));
        if (m_cloudTaaHistory[1]) m_cloudTaaHistory[1]->resize(std::max(1u, width / 4), std::max(1u, height / 4));
        if (m_postBuffer) m_postBuffer->resize(width, height);
    }
}


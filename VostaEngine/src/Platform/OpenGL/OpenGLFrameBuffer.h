#pragma once
#include "Renderer/Framebuffer.h"
#include "Renderer/Texture.h"

namespace ve {

    class OpenGLFramebuffer : public Framebuffer {
    public:
        struct ColorAttachmentData {
            uint32_t textureID = 0;
            Ref<Texture2D> texture;
        };
    public:
        OpenGLFramebuffer(uint32_t width, uint32_t height, Format format);
        OpenGLFramebuffer(const FramebufferSpec& spec);
        ~OpenGLFramebuffer();

        void createFromSpec();

        void finalizeDrawBuffers();

        void bind() override;
        void unbind() override;

        void resize(uint32_t width, uint32_t height) override;

        void attachColorTexture(uint32_t attachmentIndex, const Ref<Texture2D>& texture);
        Ref<Texture2D> getColorTexture(uint32_t attachmentIndex) const;
        Ref<Texture2D> getDepthTexture() const;

        void attachCubemapFace(uint32_t attachmentIndex, uint32_t face,
            const Ref<TextureCubeMap>& cubemap, uint32_t mipLevel = 0) override;

        uint32_t getColorAttachmentRendererID() const override {
            return m_colorData.empty() ? 0 : m_colorData[0].textureID;
        }
        uint32_t getWidth() const override { return m_width; }
        uint32_t getHeight() const override { return m_height; }

        int32_t readPixel(uint32_t x, uint32_t y) const override;

    private:
        void createBuffers();

        uint32_t m_rendererID = 0;
        uint32_t m_depthAttachment = 0;

        uint32_t m_width;
        uint32_t m_height;

        bool m_useSpec = false;
        FramebufferSpec m_spec;
        Format m_format;

        std::vector<ColorAttachmentData> m_colorData;
        std::unordered_map<uint32_t, Ref<Texture2D>> m_attachedTextures;
        Ref<Texture2D> m_depthTexture;
    };

}

#pragma once
#include "Core/Core.h"
#include "Renderer/Texture.h"

#include <cstdint>
#include <glm.hpp>

namespace ve {

    enum class TextureInternalFormat : uint8_t {
        RGBA8,
        RGBA16F,
        R32I
    };

    enum class TextureDataFormat : uint8_t {
        RGBA,
        RED_INTEGER
    };

    enum class TextureDataType : uint8_t {
        UNSIGNED_BYTE,
        FLOAT,
        INT
    };

    enum class TextureFilter : uint8_t {
        LINEAR,
        NEAREST
    };

    struct FramebufferAttachmentSpec {
        TextureInternalFormat internalFormat = TextureInternalFormat::RGBA8;
        TextureDataFormat format = TextureDataFormat::RGBA;
        TextureDataType type = TextureDataType::UNSIGNED_BYTE;
        TextureFilter minFilter = TextureFilter::LINEAR;
        TextureFilter magFilter = TextureFilter::LINEAR;
    };

    struct FramebufferSpec
    {
        uint32_t width;
        uint32_t height;
        std::vector<FramebufferAttachmentSpec> colorAttachments;
        bool hasDepthStencil = true;
        // Put the depth attachment in compare mode so it can be sampled as a
        // sampler2DShadow (hardware depth test on each tap). Used by shadow maps.
        bool depthCompare = false;
    };

    class VE_API Framebuffer {
    public:
        enum class Format {
            Color,   // RGBA8
            Picking  // R32I
        };

    public:
        virtual ~Framebuffer() = default;

        virtual void bind() = 0;
        virtual void unbind() = 0;

        // Copy the color attachment into the default (window) framebuffer,
        // scaling to the destination size. Presents an offscreen view to the
        // screen; the depth attachment is not copied.
        virtual void blitToDefault(uint32_t dstWidth, uint32_t dstHeight) = 0;

        virtual void resize(uint32_t width, uint32_t height) = 0;

        virtual uint32_t getColorAttachmentRendererID() const = 0;
        virtual void attachColorTexture(uint32_t attachmentIndex, const Ref<Texture2D>& texture) = 0;
        virtual Ref<Texture2D> getColorTexture(uint32_t attachmentIndex) const = 0;
        virtual Ref<Texture2D> getDepthTexture() const = 0;

        // Attach a single face of a cubemap to a color attachment slot.
        // Replaces whatever texture is currently bound at that attachment index.
        virtual void attachCubemapFace(uint32_t attachmentIndex, uint32_t face,
            const Ref<TextureCubeMap>& cubemap, uint32_t mipLevel = 0) = 0;

        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;

        virtual int32_t readPixel(uint32_t x, uint32_t y) const = 0;

        static Ref<Framebuffer> create(uint32_t width, uint32_t height);
        static Ref<Framebuffer> create(uint32_t width, uint32_t height, Format format);
        static Ref<Framebuffer> create(const FramebufferSpec& spec);

    };

}

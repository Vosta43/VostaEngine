#pragma once
#include "Core/Core.h"
#include "Renderer/Texture.h"

#include <cstdint>
#include <glm.hpp>

namespace ve {

    enum class TextureInternalFormat : uint8_t {
        RGBA8,
        RGBA16F,
        R32I,
        // Single-channel 16-bit unsigned integer. The RVT page table: each texel
        // holds a tile-cache array-layer index, which needs exact integers over the
        // full range, not float precision.
        R16UI,
        // Single-channel 32-bit float, for passes that write one value per texel
        // (a Hi-Z pyramid) and have it read back as floats.
        R32F
    };

    enum class TextureDataFormat : uint8_t {
        RGBA,
        RED_INTEGER,
        RED
    };

    enum class TextureDataType : uint8_t {
        UNSIGNED_BYTE,
        FLOAT,
        INT,
        UNSIGNED_SHORT
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
        // Float32 depth is the engine's choice: reverse-Z relies on the exponent's
        // log distribution matching depth's, which only a float format has. A
        // consumer on standard depth that wants stencil in the same attachment
        // asks for Depth24Stencil8 instead.
        enum class DepthFormat : uint8_t {
            Depth32F,
            Depth24Stencil8
        };

        uint32_t width;
        uint32_t height;
        std::vector<FramebufferAttachmentSpec> colorAttachments;
        bool hasDepth = true;
        DepthFormat depthFormat = DepthFormat::Depth32F;
        // False stores depth in a renderbuffer: cheaper, but the depth cannot be
        // sampled or read back (getDepthTexture() returns null).
        bool depthAsTexture = true;
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

        // A rectangle in framebuffer pixels, origin bottom-left.
        struct BlitRect {
            int32_t x = 0;
            int32_t y = 0;
            int32_t width = 0;
            int32_t height = 0;
        };

        virtual void bind() = 0;
        virtual void unbind() = 0;

        // Copy the color attachment into the default (window) framebuffer,
        // scaling to the destination size. Presents an offscreen view to the
        // screen; the depth attachment is not copied.
        virtual void blitToDefault(uint32_t dstWidth, uint32_t dstHeight) = 0;

        // Copy a rectangle of this target into `dst` (null = the default
        // framebuffer). A depth blit always resolves with nearest filtering --
        // the API forbids filtering depth -- and requires both targets to carry
        // the same depth format.
        virtual void blitTo(Framebuffer* dst, const BlitRect& srcRect, const BlitRect& dstRect,
                            bool color, bool depth, TextureFilter filter = TextureFilter::NEAREST) = 0;

        virtual uint32_t getRendererID() const = 0;

        virtual void resize(uint32_t width, uint32_t height) = 0;

        virtual uint32_t getColorAttachmentRendererID() const = 0;
        virtual void attachColorTexture(uint32_t attachmentIndex, const Ref<Texture2D>& texture) = 0;
        virtual Ref<Texture2D> getColorTexture(uint32_t attachmentIndex) const = 0;
        virtual Ref<Texture2D> getDepthTexture() const = 0;

        // Attach a single face of a cubemap to a color attachment slot.
        // Replaces whatever texture is currently bound at that attachment index.
        virtual void attachCubemapFace(uint32_t attachmentIndex, uint32_t face,
            const Ref<TextureCubeMap>& cubemap, uint32_t mipLevel = 0) = 0;

        // Attach one layer (slice) of a 2D texture array to a color attachment slot.
        // The RVT bake pass renders each page into a layer of the tile cache, so the
        // attachment is re-pointed at a new layer per page rather than at a whole
        // texture.
        virtual void attachColorTextureLayer(uint32_t attachmentIndex, uint32_t layer,
            const Ref<Texture2DArray>& array, uint32_t mipLevel = 0) = 0;

        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;

        virtual int32_t readPixel(uint32_t x, uint32_t y) const = 0;

        // Synchronously reads a rectangle of color attachment 0 as tightly packed
        // RGBA8 into `out` (w*h*4 bytes). Meant for byte-format targets
        // (screenshots, icon thumbnails): an integer attachment cannot be read
        // this way, and a float target is converted on read.
        virtual void readPixels(uint32_t x, uint32_t y, uint32_t w, uint32_t h, void* out) const = 0;

        static Ref<Framebuffer> create(uint32_t width, uint32_t height);
        static Ref<Framebuffer> create(uint32_t width, uint32_t height, Format format);
        static Ref<Framebuffer> create(const FramebufferSpec& spec);

    };

}

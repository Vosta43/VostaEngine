#include "vepch.h"
#include "OpenGLTexture.h"
#include "Core/Log.h"
#include "Math/Math.h"
#include <glad/glad.h>
#include "stb_image.h"

#include <cmath>

namespace ve {

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
        : m_path(path)
        , m_ownsTexture(true)
        , m_format(TextureFormat::RGBA) {

        int width, height, channels;

        // Vertically flip the image on load, because OpenGL expects texture origin at bottom-left, while most image formats define origin at top-left.
        stbi_set_flip_vertically_on_load(1);
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (!data) {
            VE_CORE_ERROR_PRINT("Failed to load texture: %s", path.c_str());
            m_width = 0;
            m_height = 0;
            m_rendererId = 0;
            return;
        }

        m_width = width;
        m_height = height;

        GLenum internalFormat = 0, dataFormat = 0;
        if (channels == 4) {
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
            m_format = TextureFormat::RGBA;
        }
        else if (channels == 3) {
            internalFormat = GL_RGB8;
            dataFormat = GL_RGB;
            m_format = TextureFormat::RGB;
        }

        // Immutable storage fixes the level count: allocate the whole chain or
        // GenerateMipmap has nowhere to write and minification aliases.
        const GLsizei mipLevels = 1 + (GLsizei)std::floor(std::log2((float)math::max(m_width, m_height)));

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, mipLevels, internalFormat, m_width, m_height);
        glTextureSubImage2D(m_rendererId, 0, 0, 0, m_width, m_height, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateTextureMipmap(m_rendererId);

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);

        stbi_image_free(data);
    }

    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
        , m_ownsTexture(true)
        , m_format(TextureFormat::RGBA) {

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, 1, GL_RGBA8, m_width, m_height);

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    static void textureFormatToOpenGL(TextureFormat tf, GLenum& outInternalFormat, GLenum& outDataFormat) {
        switch (tf) {
        case TextureFormat::RGBA:
            outInternalFormat = GL_RGBA8;
            outDataFormat = GL_RGBA;
            break;
        case TextureFormat::RGB:
            outInternalFormat = GL_RGB8;
            outDataFormat = GL_RGB;
            break;
        case TextureFormat::SRGB8:
            outInternalFormat = GL_SRGB8;
            outDataFormat = GL_RGB;
            break;
        case TextureFormat::SRGB8_ALPHA8:
            outInternalFormat = GL_SRGB8_ALPHA8;
            outDataFormat = GL_RGBA;
            break;
        case TextureFormat::R8:
            outInternalFormat = GL_R8;
            outDataFormat = GL_RED;
            break;
        case TextureFormat::R16UI:
            outInternalFormat = GL_R16UI;
            outDataFormat = GL_RED_INTEGER;
            break;
        case TextureFormat::R32UI:
            outInternalFormat = GL_R32UI;
            outDataFormat = GL_RED_INTEGER;
            break;
        case TextureFormat::RG16F:
            outInternalFormat = GL_RG16F;
            outDataFormat = GL_RG;
            break;
        case TextureFormat::RGB16F:
            outInternalFormat = GL_RGB16F;
            outDataFormat = GL_RGB;
            break;
        case TextureFormat::RGBA16F:
            outInternalFormat = GL_RGBA16F;
            outDataFormat = GL_RGBA;
            break;
        case TextureFormat::R16F:
            outInternalFormat = GL_R16F;
            outDataFormat = GL_RED;
            break;
        case TextureFormat::NONE:
        default:
            outInternalFormat = 0;
            outDataFormat = 0;
            VE_CORE_ERROR_PRINT("Unsupported TextureFormat for OpenGL mapping");
            break;
        }
    }

    // Component type for pixel upload: float formats are uploaded as GL_FLOAT,
    // byte formats as GL_UNSIGNED_BYTE.
    static GLenum textureFormatToOpenGLType(TextureFormat tf) {
        switch (tf) {
        case TextureFormat::RG16F:
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F:
        case TextureFormat::R16F:
            return GL_FLOAT;
        case TextureFormat::R16UI:
            return GL_UNSIGNED_SHORT;
        case TextureFormat::R32UI:
            return GL_UNSIGNED_INT;
        default:
            return GL_UNSIGNED_BYTE;
        }
    }

    // The image-unit format qualifier for a texture format. It must name the same
    // internal format the texture was created with: the compiler derives the type
    // imageLoad returns from it, so there is no conversion to fall back on.
    static GLenum textureFormatToImageFormat(TextureFormat tf) {
        switch (tf) {
        case TextureFormat::R8:           return GL_R8;
        case TextureFormat::R16F:         return GL_R16F;
        case TextureFormat::R16UI:        return GL_R16UI;
        case TextureFormat::R32UI:        return GL_R32UI;
        case TextureFormat::RG16F:        return GL_RG16F;
        case TextureFormat::RGB16F:       return GL_RGB16F;
        case TextureFormat::RGBA16F:      return GL_RGBA16F;
        case TextureFormat::SRGB8:        return GL_SRGB8;
        case TextureFormat::SRGB8_ALPHA8: return GL_SRGB8_ALPHA8;
        case TextureFormat::RGB:          return GL_RGB8;
        case TextureFormat::RGBA:         return GL_RGBA8;
        default:                          return GL_RGBA8;
        }
    }

    static GLenum imageAccessToOpenGL(ImageAccess access) {
        switch (access) {
        case ImageAccess::Read:      return GL_READ_ONLY;
        case ImageAccess::Write:     return GL_WRITE_ONLY;
        case ImageAccess::ReadWrite: return GL_READ_WRITE;
        }
        return GL_READ_WRITE;
    }

    static void bindImageToUnit(uint32_t unit, uint32_t rendererId, TextureFormat format,
        ImageAccess access, int level, bool layered, int layer) {
        glBindImageTexture(unit, rendererId, level, layered ? GL_TRUE : GL_FALSE, layer,
            imageAccessToOpenGL(access), textureFormatToImageFormat(format));
    }

    static void unbindImageFromUnit(uint32_t unit) {
        // Image unit 0 with texture 0 clears the binding; the format is ignored.
        glBindImageTexture(unit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    }

    OpenGLTexture2D::OpenGLTexture2D(Ref<TextureResource> textureResource)
        : m_width(0)
        , m_height(0)
        , m_ownsTexture(true)
        , m_rendererId(0)
        , m_format(TextureFormat::RGBA) {

        if (!textureResource) {
            VE_CORE_ERROR_PRINT("Failed to create OpenGLTexture2D");
            return;
        }
        m_width = textureResource->width;
        m_height = textureResource->height;
        m_format = textureResource->format;

        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(textureResource->format, internalFormat, dataFormat);

        bool isHDR = (textureResource->format == TextureFormat::RGB16F ||
                      textureResource->format == TextureFormat::RGBA16F ||
                      textureResource->format == TextureFormat::RG16F);

        VE_CORE_SUCCESS_PRINT("OpenGL texture created: rendererID=%u, %dx%d, internalFormat=0x%X",
            m_rendererId, textureResource->width, textureResource->height, internalFormat);

        // Immutable storage fixes the level count: allocate the whole chain or
        // GenerateMipmap has nowhere to write and minification aliases.
        const GLsizei mipLevels = 1 + (GLsizei)std::floor(std::log2((float)math::max(m_width, m_height)));

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, mipLevels, internalFormat, m_width, m_height);
        glTextureSubImage2D(m_rendererId, 0, 0, 0, textureResource->width, textureResource->height,
            dataFormat, isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE,
            isHDR ? (const void*)textureResource->floatPixels.data() : (const void*)textureResource->pixels.data());
        glGenerateTextureMipmap(m_rendererId);

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);

    }

    OpenGLTexture2D::OpenGLTexture2D(uint32_t rendererID, uint32_t width, uint32_t height, bool ownsTexture)
        : m_width(width)
        , m_height(height)
        , m_rendererId(rendererID)
        , m_ownsTexture(ownsTexture)
        , m_format(TextureFormat::RGBA)
    {
    }

    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, TextureFormat format)
        : OpenGLTexture2D(width, height, format, 1) {
    }

    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, TextureFormat format, uint32_t mipLevels)
        : m_width(width)
        , m_height(height)
        , m_ownsTexture(true)
        , m_format(format) {

        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(format, internalFormat, dataFormat);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, mipLevels ? (GLsizei)mipLevels : 1, internalFormat, m_width, m_height);

        // Integer formats (R16UI) have no linear filter in GL; asking for one is an
        // error. The page table is read as an exact index, so nearest is also what
        // the sampler wants.
        const bool integerFormat = (format == TextureFormat::R16UI || format == TextureFormat::R32UI);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, integerFormat ? GL_NEAREST : GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, integerFormat ? GL_NEAREST : GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Pin anisotropic filtering to 1 (the spec default). The R16F weather map
        // is 1:1-ish and gains nothing from AF; a driver that force-enables AF on
        // textures it deems "data", or the driver's forced-AF override, would make
        // the filter state on this texture driver-dependent. Stating 1.0 keeps the
        // sample path identical across GPUs.
        glTextureParameterf(m_rendererId, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
    }

    OpenGLTexture2D::~OpenGLTexture2D() {
        // Only delete the GL name when this wrapper actually owns it. The FBO
        // wraps its own color/depth textures with ownsTexture=false (see
        // Texture2D::create(rendererID, w, h)) and is the sole owner of those
        // names, so a non-owning wrapper must not delete them too.
        if (m_ownsTexture && m_rendererId != 0) {
            glDeleteTextures(1, &m_rendererId);
        }
    }

    void OpenGLTexture2D::setData(void* data, uint32_t size) {
        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(m_format, internalFormat, dataFormat);
        glTextureSubImage2D(m_rendererId, 0, 0, 0, m_width, m_height,
                            dataFormat, textureFormatToOpenGLType(m_format), data);
    }

    void OpenGLTexture2D::setSubData(const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(m_format, internalFormat, dataFormat);
        glTextureSubImage2D(m_rendererId, 0, x, y, w, h,
                            dataFormat, textureFormatToOpenGLType(m_format), data);
    }

    void OpenGLTexture2D::setDataLevel(void* data, uint32_t size, uint32_t mip) {
        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(m_format, internalFormat, dataFormat);
        // Each level halves, floored at one texel -- the same rule glTextureStorage2D
        // allocated the chain with, so the upload always matches the level's extent.
        const uint32_t w = math::max(1u, m_width  >> mip);
        const uint32_t h = math::max(1u, m_height >> mip);
        glTextureSubImage2D(m_rendererId, (GLint)mip, 0, 0, w, h,
                            dataFormat, textureFormatToOpenGLType(m_format), data);
    }

    void OpenGLTexture2D::bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_rendererId);
    }

    void OpenGLTexture2D::bindImage(uint32_t unit, ImageAccess access, int level, bool layered, int layer) const {
        bindImageToUnit(unit, m_rendererId, m_format, access, level, layered, layer);
    }

    void OpenGLTexture2D::unbindImage(uint32_t unit) const {
        unbindImageFromUnit(unit);
    }

    OpenGLTextureCube::OpenGLTextureCube(Ref<TextureCubeMapResource> resource) {
        if (!resource) {
            VE_CORE_ERROR_PRINT("Null TextureCubeMapResource");
            return;
        }

        m_path = resource->sourceFilePathPath;
        m_width = resource->width;
        m_height = resource->height;
        m_format = resource->format;

        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(resource->format, internalFormat, dataFormat);


        uint32_t faceSize = resource->width / 4;

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, 1, internalFormat, faceSize, faceSize);

        for (int face = 0; face < 6; ++face) {
            glTextureSubImage3D(
                m_rendererId,
                0,                       // mip level
                0, 0,                    // x, y offset
                face,                    // z offset (face index)
                faceSize, faceSize,
                1,                       // depth = 1
                dataFormat,
                GL_FLOAT,
                (const void*)resource->facePixels[face].data()
            );
        }

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    OpenGLTextureCube::OpenGLTextureCube(uint32_t faceSize)
        : m_width(faceSize)
        , m_height(faceSize)
        , m_format(TextureFormat::RGBA16F) {

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, 1, GL_RGBA16F, faceSize, faceSize);

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    OpenGLTextureCube::OpenGLTextureCube(uint32_t faceSize, uint32_t mipLevels)
        : m_width(faceSize)
        , m_height(faceSize)
        , m_format(TextureFormat::RGBA16F) {

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, mipLevels, GL_RGBA16F, faceSize, faceSize);

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    // Every OpenGLTextureCube creates its own GL cubemap; the FBO only binds
    // faces to it (attachCubemapFace) and never takes ownership, so the wrapper
    // is always the owner and must free the name.
    OpenGLTextureCube::~OpenGLTextureCube() {
        if (m_rendererId != 0) {
            glDeleteTextures(1, &m_rendererId);
        }
    }

    void OpenGLTextureCube::bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_rendererId);
    }

    void OpenGLTextureCube::bindImage(uint32_t unit, ImageAccess access, int level, bool layered, int layer) const {
        bindImageToUnit(unit, m_rendererId, m_format, access, level, layered, layer);
    }

    void OpenGLTextureCube::unbindImage(uint32_t unit) const {
        unbindImageFromUnit(unit);
    }

    void OpenGLTextureCube::setData(void* data, uint32_t size) {
        // CubeMap faces are write-once during construction
    }

    OpenGLTexture2DArray::OpenGLTexture2DArray(uint32_t width, uint32_t height, uint32_t layers,
        TextureFormat format, const void* data)
        : m_width(width)
        , m_height(height)
        , m_layers(layers)
        , m_rendererId(0)
        , m_format(format) {

        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(format, internalFormat, dataFormat);

        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_rendererId);
        glTextureStorage3D(m_rendererId, 1, internalFormat, m_width, m_height, m_layers);
        if (data) {
            glTextureSubImage3D(m_rendererId, 0, 0, 0, 0, m_width, m_height, m_layers,
                                dataFormat, textureFormatToOpenGLType(format), data);
        }

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Explicit AF=1, the spec default: see the R16F Texture2D ctor.
        glTextureParameterf(m_rendererId, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
    }

    OpenGLTexture2DArray::~OpenGLTexture2DArray() {
        if (m_rendererId != 0) {
            glDeleteTextures(1, &m_rendererId);
        }
    }

    void OpenGLTexture2DArray::setData(void* data, uint32_t size) {
        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(m_format, internalFormat, dataFormat);
        glTextureSubImage3D(m_rendererId, 0, 0, 0, 0, m_width, m_height, m_layers,
                            dataFormat, textureFormatToOpenGLType(m_format), data);
    }

    void OpenGLTexture2DArray::bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_rendererId);
    }

    void OpenGLTexture2DArray::bindImage(uint32_t unit, ImageAccess access, int level, bool layered, int layer) const {
        bindImageToUnit(unit, m_rendererId, m_format, access, level, layered, layer);
    }

    void OpenGLTexture2DArray::unbindImage(uint32_t unit) const {
        unbindImageFromUnit(unit);
    }

    OpenGLTexture3D::OpenGLTexture3D(uint32_t width, uint32_t height, uint32_t depth, const float* data)
        : m_width(width)
        , m_height(height)
        , m_depth(depth)
        , m_rendererId(0)
        , m_format(TextureFormat::R16F) {

        glCreateTextures(GL_TEXTURE_3D, 1, &m_rendererId);
        glTextureStorage3D(m_rendererId, 1, GL_R16F, m_width, m_height, m_depth);
        if (data) {
            glTextureSubImage3D(m_rendererId, 0, 0, 0, 0, m_width, m_height, m_depth,
                                GL_RED, GL_FLOAT, data);
        }

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_R, GL_REPEAT);
        // Explicit AF=1, the spec default: see the R16F Texture2D ctor.
        glTextureParameterf(m_rendererId, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
    }

    OpenGLTexture3D::OpenGLTexture3D(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, const float* data, bool repeatWrap)
        : m_width(width)
        , m_height(height)
        , m_depth(depth)
        , m_rendererId(0)
        , m_format(format)
        , m_repeatWrap(repeatWrap) {

        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(format, internalFormat, dataFormat);

        glCreateTextures(GL_TEXTURE_3D, 1, &m_rendererId);
        glTextureStorage3D(m_rendererId, 1, internalFormat, m_width, m_height, m_depth);
        if (data) {
            glTextureSubImage3D(m_rendererId, 0, 0, 0, 0, m_width, m_height, m_depth,
                                dataFormat, textureFormatToOpenGLType(format), data);
        }

        const GLint wrap = m_repeatWrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, wrap);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, wrap);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_R, wrap);
        // Explicit AF=1, the spec default: see the R16F Texture2D ctor.
        glTextureParameterf(m_rendererId, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
    }

    OpenGLTexture3D::~OpenGLTexture3D() {
        if (m_rendererId != 0) {
            glDeleteTextures(1, &m_rendererId);
        }
    }

    void OpenGLTexture3D::setData(void* data, uint32_t size) {
        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(m_format, internalFormat, dataFormat);
        glTextureSubImage3D(m_rendererId, 0, 0, 0, 0, m_width, m_height, m_depth,
                            dataFormat, textureFormatToOpenGLType(m_format), data);
    }

    void OpenGLTexture3D::bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_3D, m_rendererId);
    }

    void OpenGLTexture3D::bindImage(uint32_t unit, ImageAccess access, int level, bool layered, int layer) const {
        bindImageToUnit(unit, m_rendererId, m_format, access, level, layered, layer);
    }

    void OpenGLTexture3D::unbindImage(uint32_t unit) const {
        unbindImageFromUnit(unit);
    }

}

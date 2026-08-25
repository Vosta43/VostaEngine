#include "vepch.h"
#include "OpenGLTexture.h"
#include "Core/Log.h"
#include <glad/glad.h>
#include "stb_image.h"

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

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, 1, internalFormat, m_width, m_height);
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
        case TextureFormat::R8:
            outInternalFormat = GL_R8;
            outDataFormat = GL_RED;
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
        default:
            return GL_UNSIGNED_BYTE;
        }
    }

    OpenGLTexture2D::OpenGLTexture2D(Ref<TextureResource> textureResource)
        : m_format(TextureFormat::RGBA) {

        if (!textureResource) {
            VE_CORE_ERROR_PRINT("Failed to create OpenGLTexture2D");
            return;
        }
        m_format = textureResource->format;

        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(textureResource->format, internalFormat, dataFormat);

        bool isHDR = (textureResource->format == TextureFormat::RGB16F ||
                      textureResource->format == TextureFormat::RGBA16F ||
                      textureResource->format == TextureFormat::RG16F);

        VE_CORE_SUCCESS_PRINT("OpenGL texture created: rendererID=%u, %dx%d, internalFormat=0x%X",
            m_rendererId, textureResource->width, textureResource->height, internalFormat);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, 1, internalFormat, textureResource->width, textureResource->height);
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
        : m_width(width)
        , m_height(height)
        , m_ownsTexture(true)
        , m_format(format) {

        GLenum internalFormat, dataFormat;
        textureFormatToOpenGL(format, internalFormat, dataFormat);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererId);
        glTextureStorage2D(m_rendererId, 1, internalFormat, m_width, m_height);

        glTextureParameteri(m_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_rendererId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

    void OpenGLTexture2D::bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_rendererId);
    }

    OpenGLTextureCube::OpenGLTextureCube(Ref<TextureCubeMapResource> resource) {
        if (!resource) {
            VE_CORE_ERROR_PRINT("Null TextureCubeMapResource");
            return;
        }

        m_path = resource->sourceFilePathPath;
        m_width = resource->width;
        m_height = resource->height;

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
        , m_height(faceSize) {

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
        , m_height(faceSize) {

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

    void OpenGLTextureCube::setData(void* data, uint32_t size) {
        // CubeMap faces are write-once during construction
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

}

#include "vepch.h"
#include "OpenGLFramebuffer.h"
#include <glad/glad.h>

namespace ve {

    OpenGLFramebuffer::OpenGLFramebuffer(uint32_t width, uint32_t height, Format format)
        : m_width(width), m_height(height), m_format(format) {
        createBuffers();
    }

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpec& spec)
        : m_width(spec.width), m_height(spec.height), m_useSpec(true), m_spec(spec) {
        createFromSpec();
    }


    OpenGLFramebuffer::~OpenGLFramebuffer() {
        // The FBO owns the GL names of its internally created color/depth
        // textures. Their Texture2D wrappers are non-owning (ownsTexture=false),
        // so the FBO must free them here; m_attachedTextures holds external
        // textures whose owners remain responsible for them.
        for (auto& data : m_colorData) {
            if (data.textureID) glDeleteTextures(1, &data.textureID);
        }
        if (m_depthTexture && m_depthTexture->getRendererID()) {
            GLuint depthID = m_depthTexture->getRendererID();
            glDeleteTextures(1, &depthID);
        }
        if (m_rendererID) glDeleteFramebuffers(1, &m_rendererID);
        if (m_depthAttachment) glDeleteRenderbuffers(1, &m_depthAttachment);
    }

    static GLenum toGLInternalFormat(TextureInternalFormat f) {
        switch (f) {
        case TextureInternalFormat::RGBA8:   return GL_RGBA8;
        case TextureInternalFormat::RGBA16F: return GL_RGBA16F;
        case TextureInternalFormat::R32I:    return GL_R32I;
        }
        return GL_RGBA8;
    }

    static GLenum toGLDataFormat(TextureDataFormat f) {
        switch (f) {
        case TextureDataFormat::RGBA:        return GL_RGBA;
        case TextureDataFormat::RED_INTEGER: return GL_RED_INTEGER;
        }
        return GL_RGBA;
    }

    static GLenum toGLDataType(TextureDataType t) {
        switch (t) {
        case TextureDataType::UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
        case TextureDataType::FLOAT:         return GL_FLOAT;
        case TextureDataType::INT:           return GL_INT;
        }
        return GL_UNSIGNED_BYTE;
    }

    static GLenum toGLFilter(TextureFilter f) {
        switch (f) {
        case TextureFilter::LINEAR:  return GL_LINEAR;
        case TextureFilter::NEAREST: return GL_NEAREST;
        }
        return GL_LINEAR;
    }

    void OpenGLFramebuffer::createFromSpec() {
        glCreateFramebuffers(1, &m_rendererID);

        const size_t count = m_spec.colorAttachments.size();
        m_colorData.resize(count);

        for (size_t i = 0; i < count; ++i) {
            const auto& attachSpec = m_spec.colorAttachments[i];
            GLuint texID = 0;
            glCreateTextures(GL_TEXTURE_2D, 1, &texID);
            glTextureStorage2D(texID, 1, toGLInternalFormat(attachSpec.internalFormat), m_width, m_height);
            glTextureParameteri(texID, GL_TEXTURE_MIN_FILTER, toGLFilter(attachSpec.minFilter));
            glTextureParameteri(texID, GL_TEXTURE_MAG_FILTER, toGLFilter(attachSpec.magFilter));
            glTextureParameteri(texID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(texID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glNamedFramebufferTexture(m_rendererID,
                GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i),
                texID, 0);

            m_colorData[i].textureID = texID;
            m_colorData[i].texture = Texture2D::create(texID, m_width, m_height);

        }

        if (m_spec.hasDepthStencil) {
            GLuint depthTexID = 0;
            glCreateTextures(GL_TEXTURE_2D, 1, &depthTexID);
            glTextureStorage2D(depthTexID, 1, GL_DEPTH24_STENCIL8, m_width, m_height);
            glTextureParameteri(depthTexID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(depthTexID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(depthTexID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(depthTexID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glNamedFramebufferTexture(m_rendererID,
                GL_DEPTH_STENCIL_ATTACHMENT,
                depthTexID, 0);

            m_depthTexture = Texture2D::create(depthTexID, m_width, m_height);
        }
        finalizeDrawBuffers();
    }

    void OpenGLFramebuffer::finalizeDrawBuffers() {
        const size_t count = m_colorData.size();
        std::vector<GLenum> drawBuffers(count);
        for (size_t i = 0; i < count; ++i) {
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
        }
        glNamedFramebufferDrawBuffers(m_rendererID,
            static_cast<GLsizei>(count),
            drawBuffers.data());
    }

    void OpenGLFramebuffer::createBuffers() {
        GLenum internalFormat = GL_RGBA8;
        GLenum minFilter = GL_LINEAR;
        GLenum magFilter = GL_LINEAR;

        if (m_format == Format::Picking) {
            internalFormat = GL_R32I;
            minFilter = GL_NEAREST;
            magFilter = GL_NEAREST;
        }

        GLuint texID = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &texID);
        glTextureStorage2D(texID, 1, internalFormat, m_width, m_height);
        glTextureParameteri(texID, GL_TEXTURE_MIN_FILTER, minFilter);
        glTextureParameteri(texID, GL_TEXTURE_MAG_FILTER, magFilter);
        glTextureParameteri(texID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLuint depthTexID = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &depthTexID);
        glTextureStorage2D(depthTexID, 1, GL_DEPTH24_STENCIL8, m_width, m_height);
        glTextureParameteri(depthTexID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(depthTexID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(depthTexID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(depthTexID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glCreateFramebuffers(1, &m_rendererID);
        glNamedFramebufferTexture(m_rendererID, GL_COLOR_ATTACHMENT0, texID, 0);
        glNamedFramebufferTexture(m_rendererID, GL_DEPTH_STENCIL_ATTACHMENT, depthTexID, 0);

        m_depthTexture = Texture2D::create(depthTexID, m_width, m_height);

        m_colorData.resize(1);
        std::vector<GLenum> drawBuffers = { GL_COLOR_ATTACHMENT0 };
        glNamedFramebufferDrawBuffers(m_rendererID, 1, drawBuffers.data());
        m_colorData[0].textureID = texID;
        m_colorData[0].texture = Texture2D::create(texID, m_width, m_height);
    }

    int32_t OpenGLFramebuffer::readPixel(uint32_t x, uint32_t y) const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_rendererID);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        int32_t pixelData = -1;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return pixelData;
    }

    void OpenGLFramebuffer::bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, m_rendererID);
        glViewport(0, 0, m_width, m_height);
    }

    void OpenGLFramebuffer::unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::resize(uint32_t width, uint32_t height) {
        m_width = width;
        m_height = height;

        for (auto& data : m_colorData) {
            if (data.textureID) glDeleteTextures(1, &data.textureID);
        }
        m_colorData.clear();
        if (m_depthTexture) {
            GLuint depthID = m_depthTexture->getRendererID();
            if (depthID) glDeleteTextures(1, &depthID);
            m_depthTexture = nullptr;
        }
        if (m_depthAttachment) {
            glDeleteRenderbuffers(1, &m_depthAttachment);
            m_depthAttachment = 0;
        }
        if (m_rendererID) {
            glDeleteFramebuffers(1, &m_rendererID);
            m_rendererID = 0;
        }

        if (m_useSpec) {
            createFromSpec();
        }
        else {
            createBuffers();
        }
    }

    void OpenGLFramebuffer::attachColorTexture(uint32_t attachmentIndex, const Ref<Texture2D>& texture)
    {
        if (!texture) return;

        glBindFramebuffer(GL_FRAMEBUFFER, m_rendererID);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0 + attachmentIndex,
            GL_TEXTURE_2D,
            texture->getRendererID(),
            0);

        m_attachedTextures[attachmentIndex] = texture;

        // Update draw buffers
        std::vector<GLenum> drawBuffers;
        for (uint32_t i = 0; i <= attachmentIndex; ++i) {
            if (m_attachedTextures.find(i) != m_attachedTextures.end()) {
                drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
            }
        }
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::attachCubemapFace(uint32_t attachmentIndex, uint32_t face,
        const Ref<TextureCubeMap>& cubemap, uint32_t mipLevel) {

        if (!cubemap) return;

        glNamedFramebufferTextureLayer(
            m_rendererID,
            GL_COLOR_ATTACHMENT0 + attachmentIndex,
            cubemap->getRendererID(),
            mipLevel,
            face
        );
    }

    Ref<Texture2D> OpenGLFramebuffer::getColorTexture(uint32_t attachmentIndex) const {
        auto it = m_attachedTextures.find(attachmentIndex);
        if (it != m_attachedTextures.end()) return it->second;

        if (attachmentIndex < m_colorData.size()) {
            return m_colorData[attachmentIndex].texture;
        }
        return nullptr;
    }

    Ref<Texture2D> OpenGLFramebuffer::getDepthTexture() const
    {
        return m_depthTexture;
    }



    Ref<Framebuffer> Framebuffer::create(uint32_t width, uint32_t height) {

        return CreateRef<OpenGLFramebuffer>(width, height, Format::Color);
    }

    Ref<Framebuffer> Framebuffer::create(uint32_t width, uint32_t height, Format format) {

        return CreateRef<OpenGLFramebuffer>(width, height, format);
    }

    Ref<Framebuffer> Framebuffer::create(const FramebufferSpec& spec)
    {
        return CreateRef<OpenGLFramebuffer>(spec);
    }


}
#include "vepch.h"
#include "OpenGLRendererAPI.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Renderer/IndirectBuffer.h"
#include "Core/Log.h"

#include <glad/glad.h>
namespace ve {

    // Indexed state stops at the attachment count the pipeline can address; the
    // mask is 8 bits wide, and GL 4.6 guarantees at least 8 draw buffers.
    static GLuint s_indexedAttachmentCount = 8;

    static GLenum toGLBlendFactor(BlendFactor factor) {
        switch (factor) {
        case BlendFactor::Zero:             return GL_ZERO;
        case BlendFactor::One:              return GL_ONE;
        case BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::SrcColor:         return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        }
        return GL_ONE;
    }

    static GLenum toGLCompareOp(CompareOp op) {
        switch (op) {
        case CompareOp::Never:    return GL_NEVER;
        case CompareOp::Less:     return GL_LESS;
        case CompareOp::LEqual:   return GL_LEQUAL;
        case CompareOp::Greater:  return GL_GREATER;
        case CompareOp::GEqual:   return GL_GEQUAL;
        case CompareOp::Equal:    return GL_EQUAL;
        case CompareOp::NotEqual: return GL_NOTEQUAL;
        case CompareOp::Always:   return GL_ALWAYS;
        }
        return GL_GREATER;
    }

    static GLenum toGLPrimitive(PrimitiveType topology) {
        switch (topology) {
        case PrimitiveType::Triangles: return GL_TRIANGLES;
        case PrimitiveType::Lines:     return GL_LINES;
        case PrimitiveType::LineStrip: return GL_LINE_STRIP;
        case PrimitiveType::Points:    return GL_POINTS;
        }
        return GL_TRIANGLES;
    }

    static GLenum toGLPolygonMode(PolygonMode mode) {
        switch (mode) {
        case PolygonMode::Fill:  return GL_FILL;
        case PolygonMode::Line:  return GL_LINE;
        case PolygonMode::Point: return GL_POINT;
        }
        return GL_FILL;
    }

    void OpenGLRendererAPI::init(bool reverseZ){
        GLint maxDrawBuffers = 0;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        s_indexedAttachmentCount = static_cast<GLuint>(maxDrawBuffers < 8 ? maxDrawBuffers : 8);

        s_reverseZ = reverseZ;

        // The state mirror describes what this function leaves GL as, so it is
        // written here rather than read back from the driver.
        m_current = RenderStateDesc{};
        m_current.depth.test = false;   // passes opt in
        m_current.depth.write = true;   // GL default depth mask
        m_current.depth.compare = reverseZ ? CompareOp::Greater : CompareOp::Less;
        m_current.blend.enable = true;  // glEnable(GL_BLEND) below; factors stay ONE/ZERO
        m_blendIndexedDirty = false;

        glEnable(GL_BLEND);

        // glClipControl goes first: it retargets the context's clip depth, which
        // every projection in Renderer/Projection.h is built for.
        glClipControl(GL_LOWER_LEFT, reverseZ ? GL_ZERO_TO_ONE : GL_NEGATIVE_ONE_TO_ONE);
        glClearDepth(reverseZ ? 0.0 : 1.0);
        glDepthFunc(reverseZ ? GL_GREATER : GL_LESS);
    }
    void OpenGLRendererAPI::setClearColor(const glm::vec4& color) {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::clear(bool color, bool depth) {
        GLbitfield mask = 0;
        if (color) mask |= GL_COLOR_BUFFER_BIT;
        if (depth) mask |= GL_DEPTH_BUFFER_BIT;
        if (mask) glClear(mask);
    }

    void OpenGLRendererAPI::clearInt(int32_t value) {
        // glClearColor is unspecified for integer color buffers; the spec-correct
        // path is glClearBufferiv. Depth is a separate attachment, so clear it
        // with the ordinary glClear.
        glClearBufferiv(GL_COLOR, 0, &value);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        //VE_CORE_SUCCESS_PRINT("glViewport called: %d %d %d %d", x, y, width, height);
        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::setScissor(int32_t x, int32_t y, int32_t width, int32_t height) {
        glScissor(x, y, width, height);
    }

    void OpenGLRendererAPI::setScissorTest(bool enabled) {
        if (enabled) {
            glEnable(GL_SCISSOR_TEST);
        }
        else {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    void OpenGLRendererAPI::drawIndexed(const Ref<VertexArray>& vertexArray) {
        vertexArray->bind();
        glDrawElements(toGLPrimitive(m_current.topology), vertexArray->getIndexBuffer()->getCount(),
                       GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::drawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount,
                                        uint32_t firstIndex, int32_t baseVertex) {
        vertexArray->bind();
        // firstIndex is an offset in indices, so scale it to bytes for the pointer offset.
        glDrawElementsBaseVertex(toGLPrimitive(m_current.topology), indexCount, GL_UNSIGNED_INT,
                                 reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex) * sizeof(uint32_t)),
                                 baseVertex);
    }

    void OpenGLRendererAPI::drawIndexedInstanced(const Ref<VertexArray>& vertexArray, uint32_t instanceCount,
                                                 uint32_t indexCount, uint32_t firstIndex, int32_t baseVertex) {
        vertexArray->bind();
        const uint32_t count = indexCount ? indexCount : vertexArray->getIndexBuffer()->getCount();
        glDrawElementsInstancedBaseVertex(toGLPrimitive(m_current.topology), count, GL_UNSIGNED_INT,
                                          reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex) * sizeof(uint32_t)),
                                          static_cast<GLsizei>(instanceCount), baseVertex);
    }

    void OpenGLRendererAPI::drawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount,
                                                uint32_t firstVertex) {
        glDrawArraysInstanced(toGLPrimitive(m_current.topology), static_cast<GLint>(firstVertex),
                              static_cast<GLsizei>(vertexCount), static_cast<GLsizei>(instanceCount));
    }

    void OpenGLRendererAPI::drawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                               const Ref<IndirectBuffer>& commands,
                                               uint32_t byteOffset) {
        vertexArray->bind();
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commands->getRendererID());
        glDrawArraysIndirect(toGLPrimitive(m_current.topology),
                             reinterpret_cast<const void*>(static_cast<uintptr_t>(byteOffset)));
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    void OpenGLRendererAPI::multiDrawArraysIndirect(const Ref<VertexArray>& vertexArray,
                                                    const Ref<IndirectBuffer>& commands,
                                                    uint32_t byteOffset, uint32_t drawCount,
                                                    uint32_t byteStride) {
        if (drawCount == 0) return;
        vertexArray->bind();
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commands->getRendererID());
        glMultiDrawArraysIndirect(toGLPrimitive(m_current.topology),
                                  reinterpret_cast<const void*>(static_cast<uintptr_t>(byteOffset)),
                                  static_cast<GLsizei>(drawCount), static_cast<GLsizei>(byteStride));
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    void OpenGLRendererAPI::drawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                                const Ref<IndirectBuffer>& commands,
                                                uint32_t byteOffset) {
        vertexArray->bind();
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commands->getRendererID());
        glDrawElementsIndirect(toGLPrimitive(m_current.topology), GL_UNSIGNED_INT,
                               reinterpret_cast<const void*>(static_cast<uintptr_t>(byteOffset)));
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    void OpenGLRendererAPI::multiDrawIndexedIndirect(const Ref<VertexArray>& vertexArray,
                                                     const Ref<IndirectBuffer>& commands,
                                                     uint32_t byteOffset, uint32_t drawCount,
                                                     uint32_t byteStride) {
        if (drawCount == 0) return;
        vertexArray->bind();
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commands->getRendererID());
        glMultiDrawElementsIndirect(toGLPrimitive(m_current.topology), GL_UNSIGNED_INT,
                                    reinterpret_cast<const void*>(static_cast<uintptr_t>(byteOffset)),
                                    static_cast<GLsizei>(drawCount), static_cast<GLsizei>(byteStride));
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    void OpenGLRendererAPI::invalidateStateCache() {
        m_stateUnknown = true;
        // External code may have left per-index blending disabled; the next
        // "all attachments" state must re-enable every index explicitly.
        m_blendIndexedDirty = true;
    }

    void OpenGLRendererAPI::applyPipelineState(const RenderStateDesc& next) {
        const RenderStateDesc& cur = m_current;
        // After invalidateStateCache() the mirror is untrustworthy: every field is
        // re-sent regardless of what the diff says.
        const bool force = m_stateUnknown;

        if (force || !(next.blend == cur.blend)) {
            if (!next.blend.enable) {
                glDisable(GL_BLEND);
                // A later "every attachment" state must clear whatever per-index
                // disables the masked path left behind.
                m_blendIndexedDirty = true;
            }
            else {
                glEnable(GL_BLEND);
                if (next.blend.attachments == 0xFF) {
                    if (m_blendIndexedDirty) {
                        for (GLuint i = 0; i < s_indexedAttachmentCount; ++i)
                            glEnablei(GL_BLEND, i);
                        m_blendIndexedDirty = false;
                    }
                }
                else {
                    for (GLuint i = 0; i < s_indexedAttachmentCount; ++i) {
                        if ((next.blend.attachments >> i) & 1u) glEnablei(GL_BLEND, i);
                        else                                    glDisablei(GL_BLEND, i);
                    }
                    m_blendIndexedDirty = true;
                }
                // Factors survive a disabled span, so they are re-sent whenever
                // blending resumes, not only when they differ.
                if (force || !cur.blend.enable || next.blend.src != cur.blend.src || next.blend.dst != cur.blend.dst)
                    glBlendFunc(toGLBlendFactor(next.blend.src), toGLBlendFactor(next.blend.dst));
            }
        }

        if (force || next.depth.test != cur.depth.test) {
            if (next.depth.test) glEnable(GL_DEPTH_TEST);
            else                 glDisable(GL_DEPTH_TEST);
        }
        // A compare chosen while testing was off is re-sent when testing resumes,
        // since GL was never told about it.
        if (next.depth.test && (force || next.depth.test != cur.depth.test ||
                                next.depth.compare != cur.depth.compare))
            glDepthFunc(toGLCompareOp(next.depth.compare));
        if (force || next.depth.write != cur.depth.write)
            glDepthMask(next.depth.write ? GL_TRUE : GL_FALSE);

        const RasterDesc& raster = next.raster;
        const RasterDesc& rasterCur = cur.raster;
        if (force || raster.cull != rasterCur.cull) {
            if (raster.cull == CullMode::None) {
                glDisable(GL_CULL_FACE);
            }
            else {
                glEnable(GL_CULL_FACE);
                glCullFace(raster.cull == CullMode::Back ? GL_BACK : GL_FRONT);
            }
        }
        if (force || raster.frontFace != rasterCur.frontFace)
            glFrontFace(raster.frontFace == FrontFace::CCW ? GL_CCW : GL_CW);
        if (force || raster.polygonMode != rasterCur.polygonMode)
            glPolygonMode(GL_FRONT_AND_BACK, toGLPolygonMode(raster.polygonMode));
        if (force || raster.depthBias != rasterCur.depthBias) {
            if (raster.depthBias) glEnable(GL_POLYGON_OFFSET_FILL);
            else                  glDisable(GL_POLYGON_OFFSET_FILL);
        }
        // Same as the depth compare: offsets set while the bias was off reach GL
        // when it turns on.
        if (raster.depthBias && (force || raster.depthBias != rasterCur.depthBias ||
                                 raster.depthBiasFactor != rasterCur.depthBiasFactor ||
                                 raster.depthBiasUnits != rasterCur.depthBiasUnits))
            glPolygonOffset(raster.depthBiasFactor, raster.depthBiasUnits);
        if (force || raster.lineWidth != rasterCur.lineWidth)
            glLineWidth(raster.lineWidth);
        if (force || raster.pointSize != rasterCur.pointSize)
            glPointSize(raster.pointSize);

        if (force || next.colorWriteMask != cur.colorWriteMask) {
            for (GLuint i = 0; i < s_indexedAttachmentCount; ++i) {
                const GLboolean on = ((next.colorWriteMask >> i) & 1u) ? GL_TRUE : GL_FALSE;
                glColorMaski(i, on, on, on, on);
            }
        }

        m_current = next;
        m_stateUnknown = false;
    }

    void OpenGLRendererAPI::setDepthTesting(bool enabled) {
        RenderStateDesc next = m_current;
        next.depth.test = enabled;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::drawArrays(uint32_t count, uint32_t firstVertex) {
        glDrawArrays(toGLPrimitive(m_current.topology), static_cast<GLint>(firstVertex),
                     static_cast<GLsizei>(count));
    }

    void OpenGLRendererAPI::setDepthFunc(CompareOp func) {
        RenderStateDesc next = m_current;
        next.depth.compare = func;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setDepthMask(bool enabled){
        RenderStateDesc next = m_current;
        next.depth.write = enabled;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setBlend(bool enabled) {
        RenderStateDesc next = m_current;
        next.blend.enable = enabled;
        // Enabling means the plain global switch, which drops any per-attachment
        // mask a previous draw left behind.
        if (enabled) next.blend.attachments = 0xFF;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setBlendFunc(BlendFactor src, BlendFactor dst) {
        RenderStateDesc next = m_current;
        next.blend.src = src;
        next.blend.dst = dst;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setBlendAttachmentMask(uint8_t attachmentMask) {
        RenderStateDesc next = m_current;
        next.blend.enable = true;
        next.blend.attachments = attachmentMask;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setColorWriteMask(uint8_t attachmentMask) {
        RenderStateDesc next = m_current;
        next.colorWriteMask = attachmentMask;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setWireframe(bool enabled) {
        RenderStateDesc next = m_current;
        next.raster.polygonMode = enabled ? PolygonMode::Line : PolygonMode::Fill;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setPolygonOffset(bool enabled, float factor, float units) {
        RenderStateDesc next = m_current;
        next.raster.depthBias = enabled;
        next.raster.depthBiasFactor = factor;
        next.raster.depthBiasUnits = units;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setDepthRange(float nearDepth01, float farDepth01) {
        // Dynamic state: not part of the pipeline state (Vulkan sets it per
        // command too), so it is not mirrored.
        glDepthRangef(nearDepth01, farDepth01);
    }

    void OpenGLRendererAPI::setLineWidth(float widthInPixels) {
        RenderStateDesc next = m_current;
        next.raster.lineWidth = widthInPixels;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setPointSize(float sizeInPixels) {
        RenderStateDesc next = m_current;
        next.raster.pointSize = sizeInPixels;
        applyPipelineState(next);
    }

    void OpenGLRendererAPI::setUnpackAlignment(uint32_t alignment) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, static_cast<GLint>(alignment));
    }

    void OpenGLRendererAPI::setPackAlignment(uint32_t alignment) {
        glPixelStorei(GL_PACK_ALIGNMENT, static_cast<GLint>(alignment));
    }


    RendererAPI* RendererAPI::create() {
        return new OpenGLRendererAPI();
    }

}
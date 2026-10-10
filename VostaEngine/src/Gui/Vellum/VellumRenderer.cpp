#include "vepch.h"
#include "VellumRenderer.h"

#include "Core/Log.h"
#include "Renderer/Buffer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RendererAPI.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/VertexArray.h"

#include <algorithm>
#include <gtc/matrix_transform.hpp>   // glm::ortho

namespace ve::vellum {

    namespace {

        // Vellum paints in a top-left origin, y-down, normalised-by-projection space.
        // Everything is a textured quad, so there is a single program.
        const char* kUIVertexSrc = R"(#version 460 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec4 a_Color;

uniform mat4 u_Projection;

out vec2 v_UV;
out vec4 v_Color;

void main() {
    v_UV = a_UV;
    v_Color = a_Color;
    gl_Position = u_Projection * vec4(a_Position, 0.0, 1.0);
}
)";

        const char* kUIFragmentSrc = R"(#version 460 core
in vec2 v_UV;
in vec4 v_Color;

uniform sampler2D u_Texture;

out vec4 FragColor;

void main() {
    FragColor = v_Color * texture(u_Texture, v_UV);
}
)";

    }

    void VellumRenderer::init() {
        if (m_initialized) return;
        m_initialized = true;

        m_shader = Shader::create("VellumUI", kUIVertexSrc, kUIFragmentSrc);
        if (!m_shader || !m_shader->isValid()) {
            VE_CORE_ERROR_PRINT("Vellum: UI shader failed to build");
            m_shader = nullptr;
            return;
        }

        m_vertexBuffer = VertexBuffer::create(nullptr, kMaxQuads * 4 * sizeof(Vertex));
        m_vertexBuffer->setLayout({
            { ShaderDataType::Float2, "a_Position" },
            { ShaderDataType::Float2, "a_UV"       },
            { ShaderDataType::Float4, "a_Color"    },
        });

        uint32_t* indices = new uint32_t[kMaxQuads * 6];
        uint32_t offset = 0;
        for (uint32_t i = 0; i < kMaxQuads * 6; i += 6) {
            indices[i + 0] = offset + 0;
            indices[i + 1] = offset + 1;
            indices[i + 2] = offset + 2;
            indices[i + 3] = offset + 2;
            indices[i + 4] = offset + 3;
            indices[i + 5] = offset + 0;
            offset += 4;
        }
        auto indexBuffer = IndexBuffer::create(indices, kMaxQuads * 6);
        delete[] indices;

        m_vertexArray = VertexArray::create();
        m_vertexArray->addVertexBuffer(m_vertexBuffer);
        m_vertexArray->setIndexBuffer(indexBuffer);

        uint8_t white[4] = { 255, 255, 255, 255 };
        m_whiteTexture = Texture2D::create(1, 1, TextureFormat::RGBA);
        m_whiteTexture->setData(white, sizeof(white));
        registerTexture(0, m_whiteTexture);
    }

    void VellumRenderer::registerTexture(TextureId id, const Ref<Texture2D>& texture) {
        if (texture) m_textures[id] = texture;
    }

    void VellumRenderer::render(const DrawList& list, const glm::vec2& surfaceSize) {
        init();
        if (!m_shader || list.commands.empty()) return;

        const uint32_t maxVertices = kMaxQuads * 4;
        const uint32_t maxIndices  = kMaxQuads * 6;
        if (list.vertices.size() > maxVertices || list.indices.size() > maxIndices) {
            VE_CORE_ERROR_PRINT("Vellum: draw list (%zu verts / %zu indices) exceeds the %u-quad capacity; frame truncated",
                                list.vertices.size(), list.indices.size(), kMaxQuads);
        }

        const uint32_t vertexCount = std::min(static_cast<uint32_t>(list.vertices.size()), maxVertices);
        m_vertexBuffer->setData(list.vertices.data(), vertexCount * static_cast<uint32_t>(sizeof(Vertex)));

        m_shader->bind();
        // ortho(0, w, h, 0) puts the origin at the top-left with y growing downwards,
        // so a widget's screen-space rect maps straight through without a y-flip.
        m_shader->setMat4("u_Projection", glm::ortho(0.0f, surfaceSize.x, surfaceSize.y, 0.0f, -1.0f, 1.0f));

        // The caller may have left a sub-rect viewport behind (editor viewports, render
        // targets); Vellum owns the whole surface it painted against.
        RenderCommand::setViewport(0, 0, static_cast<uint32_t>(surfaceSize.x), static_cast<uint32_t>(surfaceSize.y));

        RenderCommand::setDepthTesting(false);
        RenderCommand::setBlend(true);
        RenderCommand::setBlendFunc(RendererAPI::BlendFunc::SrcAlpha, RendererAPI::BlendFunc::OneMinusSrcAlpha);
        RenderCommand::setScissorTest(true);

        TextureId boundTexture = ~static_cast<TextureId>(0);
        for (const DrawCommand& cmd : list.commands) {
            if (cmd.indexCount == 0) continue;
            if (cmd.firstIndex + cmd.indexCount > maxIndices) continue;   // past what was uploaded

            if (cmd.texture != boundTexture) {
                const auto it = m_textures.find(cmd.texture);
                m_shader->setTexture("u_Texture", it != m_textures.end() ? it->second : m_whiteTexture, 0);
                boundTexture = cmd.texture;
            }

            // glScissor's origin is bottom-left, so the top-left clip rect is flipped.
            RenderCommand::setScissor(
                static_cast<int32_t>(cmd.clip.pos.x),
                static_cast<int32_t>(surfaceSize.y - (cmd.clip.pos.y + cmd.clip.size.y)),
                static_cast<int32_t>(cmd.clip.size.x),
                static_cast<int32_t>(cmd.clip.size.y));

            RenderCommand::drawIndexed(m_vertexArray, cmd.indexCount, cmd.firstIndex);
        }

        RenderCommand::setScissorTest(false);
        RenderCommand::setDepthTesting(true);
        // Restore blending too: the deferred 3D passes draw opaque and rely on it
        // being off, so leaving it on silently discards their G-buffer attachments.
        RenderCommand::setBlend(false);
    }

}

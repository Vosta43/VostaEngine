#include "TexturePreviewPanel.h"

#include "imgui.h"

#include <algorithm>
#include <cstdint>

namespace ve {

	namespace {

		const char* kPreviewVS = R"(#version 460 core
layout(location = 0) in vec2 a_Position;
out vec2 v_TexCoord;
void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord  = a_Position * 0.5 + 0.5;
}
)";

		const char* kPreviewFS = R"(#version 460 core
in vec2 v_TexCoord;
uniform sampler2D u_Texture;
uniform vec3 u_Mask;       // 1 keeps a channel, 0 zeroes it
uniform int  u_AlphaMode;  // 1 shows the alpha channel as white
layout(location = 0) out vec4 o_Color;
void main() {
    vec4 t = texture(u_Texture, v_TexCoord);
    if (u_AlphaMode != 0)
        o_Color = vec4(vec3(t.a), 1.0);
    else
        o_Color = vec4(t.rgb * u_Mask, 1.0);
}
)";

		// A channel toggle as a lettered button: tinted when on, grey when off.
		void channelButton(const char* label, bool& on, ImVec4 onColor) {
			const ImVec4 off(0.22f, 0.22f, 0.24f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Button,        on ? onColor : off);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? onColor : ImVec4(0.32f, 0.32f, 0.35f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive,  on ? onColor : ImVec4(0.16f, 0.16f, 0.18f, 1.0f));
			if (ImGui::Button(label, ImVec2(34.0f, 0.0f)))
				on = !on;
			ImGui::PopStyleColor(3);
		}

	}

	void TexturePreviewPanel::open(Ref<Texture2D> texture, const std::string& title) {
		m_texture = std::move(texture);
		m_title = title;
	}

	void TexturePreviewPanel::ensureResources() {
		if (!m_shader)
			m_shader = Shader::create("TexturePreview", kPreviewVS, kPreviewFS);

		if (!m_quad) {
			float    verts[]   = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
			uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

			m_quad = VertexArray::create();
			auto vb = VertexBuffer::create(verts, sizeof(verts));
			vb->setLayout({ { ShaderDataType::Float2, "a_Position" } });
			m_quad->addVertexBuffer(vb);
			m_quad->setIndexBuffer(IndexBuffer::create(indices, 6));
		}
	}

	void TexturePreviewPanel::renderPreview(uint32_t width, uint32_t height) {
		if (!m_texture || !m_shader)
			return;
		if (!m_target || m_target->getWidth() != width || m_target->getHeight() != height)
			m_target = Framebuffer::create(width, height);

		m_target->bind();
		RenderCommand::setViewport(0, 0, width, height);
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		RenderCommand::clear();

		// A only matters on its own: with any of RGB on the view is the masked
		// colour, so toggling A then has no colour to add.
		const bool alphaMode = m_showA && !(m_showR || m_showG || m_showB);

		m_shader->bind();
		m_shader->setTexture("u_Texture", m_texture, 0);
		m_shader->setFloat3("u_Mask", glm::vec3(m_showR ? 1.0f : 0.0f,
		                                       m_showG ? 1.0f : 0.0f,
		                                       m_showB ? 1.0f : 0.0f));
		m_shader->setInt("u_AlphaMode", alphaMode ? 1 : 0);

		m_quad->bind();
		RenderCommand::drawIndexed(m_quad);
		m_quad->unbind();

		m_shader->unbind();
		m_target->unbind();
	}

	void TexturePreviewPanel::onGuiRender(bool* openFlag) {
		const std::string title = (m_title.empty() ? std::string("Texture")
		                                           : "Texture - " + m_title) + "###texturePreview";

		ImGui::SetNextWindowSize(ImVec2(560.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title.c_str(), openFlag)) {
			ImGui::End();
			return;
		}

		if (!m_texture || m_texture->getRendererID() == 0) {
			ImGui::TextDisabled("No texture loaded.");
			ImGui::End();
			return;
		}

		channelButton("R", m_showR, ImVec4(0.80f, 0.20f, 0.20f, 1.0f));
		ImGui::SameLine();
		channelButton("G", m_showG, ImVec4(0.20f, 0.75f, 0.20f, 1.0f));
		ImGui::SameLine();
		channelButton("B", m_showB, ImVec4(0.25f, 0.35f, 0.90f, 1.0f));
		ImGui::SameLine();
		channelButton("A", m_showA, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));

		ImGui::SameLine();
		ImGui::TextDisabled("%u x %u", m_texture->getWidth(), m_texture->getHeight());

		ImGui::Separator();
		ensureResources();

		// Keep the offscreen target at most 1024 on its long edge: a 4K source
		// stays cheap, and the aspect is preserved so the image is not stretched.
		const uint32_t tw = m_texture->getWidth();
		const uint32_t th = m_texture->getHeight();
		const float    scale = std::min(1.0f, 1024.0f / (float)std::max(tw, th));
		const uint32_t rw = std::max(1u, (uint32_t)(tw * scale));
		const uint32_t rh = std::max(1u, (uint32_t)(th * scale));
		renderPreview(rw, rh);

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float  fit = std::min(avail.x / (float)tw, avail.y / (float)th);
		ImGui::Image((ImTextureID)(uintptr_t)m_target->getColorAttachmentRendererID(),
		             ImVec2(tw * fit, th * fit), ImVec2(0, 1), ImVec2(1, 0));

		ImGui::End();
	}

}

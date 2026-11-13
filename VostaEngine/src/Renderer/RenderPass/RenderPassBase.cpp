#include "vepch.h"
#include "RenderPassBase.h"
#include "Core/Log.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"
#include "Renderer/VertexArray.h"

namespace ve {

	namespace {

		RenderStateDesc mergeState(const PassStateDef& d, RenderStateDesc s) {
			if (d.blend)      { s.blend.enable = *d.blend; if (*d.blend) s.blend.attachments = 0xFF; }
			if (d.src)        s.blend.src = *d.src;
			if (d.dst)        s.blend.dst = *d.dst;
			if (d.depthTest)  s.depth.test = *d.depthTest;
			if (d.depthWrite) s.depth.write = *d.depthWrite;
			if (d.depthFunc)  s.depth.compare = *d.depthFunc;
			return s;
		}

	} // namespace

	Ref<VertexArray> RenderPassBase::s_fullscreenQuad;

	void RenderPassBase::init()
	{

	}

	void RenderPassBase::drawFullscreenQuad() {
		if (!s_fullscreenQuad) {
			float v[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
			uint32_t i[] = { 0, 1, 2, 2, 3, 0 };
			s_fullscreenQuad = VertexArray::create();
			auto vb = VertexBuffer::create(v, sizeof(v));
			vb->setLayout({ { ShaderDataType::Float2, "a_Position" } });
			s_fullscreenQuad->addVertexBuffer(vb);
			s_fullscreenQuad->setIndexBuffer(IndexBuffer::create(i, 6));
		}
		s_fullscreenQuad->bind();
		RenderCommand::drawIndexed(s_fullscreenQuad);
		s_fullscreenQuad->unbind();
	}

	bool RenderPassBase::beginPass(RenderContext& ctx, const Ref<Framebuffer>& overrideTarget) {
		Ref<Framebuffer> target = overrideTarget;
		if (!target)
			target = (m_targetsDefault || m_targetsOutput) ? ctx.outputFrameBuffer : m_target;
		if (!target) return false;

		m_boundTarget = target;
		target->bind();

		if (m_def.hasViewport) {
			if (m_def.viewport.mode == PassViewport::Mode::Frame) {
				RenderCommand::setViewport((uint32_t)ctx.viewPortX, (uint32_t)ctx.viewPortY,
				                           (uint32_t)ctx.viewPortWidth, (uint32_t)ctx.viewPortHeight);
			}
			else if (m_def.viewport.mode == PassViewport::Mode::Explicit) {
				RenderCommand::setViewport((uint32_t)m_def.viewport.x, (uint32_t)m_def.viewport.y,
				                           m_def.viewport.width, m_def.viewport.height);
			}
		}

		if (m_def.hasState)
			RenderCommand::applyPipelineState(mergeState(m_def.state, RenderCommand::currentState()));

		if (m_def.clear.enabled) {
			const ClearDef& c = m_def.clear;
			RenderCommand::setClearColor(c.color[0], c.color[1], c.color[2], c.color[3]);
			RenderCommand::clear(true, c.depth);
		}
		return true;
	}

	void RenderPassBase::configure(const PassDef& def,
	                               const std::unordered_map<std::string, Ref<Framebuffer>>& fbos,
	                               const Ref<Shader>& shader)
	{
		m_def = def;
		m_name = def.name;
		m_type = def.type;
		m_shader = shader;
		m_target.reset();
		m_history[0].reset();
		m_history[1].reset();
		m_written.reset();

		m_targetsDefault = (def.target == "@default");
		m_targetsOutput = (def.target == "@output");
		if (!m_targetsDefault && !m_targetsOutput && !def.target.empty()) {
			auto it = fbos.find(def.target);
			if (it != fbos.end())
				m_target = it->second;
			else
				VE_CORE_WARN_PRINT("RenderPass '%s': target '%s' is not a known framebuffer", m_name.c_str(), def.target.c_str());
		}

		if (def.history.size() >= 2) {
			auto a = fbos.find(def.history[0]);
			auto b = fbos.find(def.history[1]);
			if (a != fbos.end()) m_history[0] = a->second;
			if (b != fbos.end()) m_history[1] = b->second;
			if (!m_history[0] || !m_history[1])
				VE_CORE_WARN_PRINT("RenderPass '%s': incomplete history pair", m_name.c_str());
		}
	}

}

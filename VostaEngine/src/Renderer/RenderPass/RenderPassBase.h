#pragma once

#include "Core/Core.h"
#include "Renderer/RenderContext.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Pipeline/PipelineConfig.h"

#include <string>
#include <unordered_map>

namespace ve {

	// Base for a pipeline stage. Wiring (target framebuffer, declared inputs,
	// ping-pong history, clear, state, gating) comes from its PassDef; the
	// subclass owns only the drawing logic in execute().
	class RenderPassBase {
	public:
		virtual ~RenderPassBase() = default;
		virtual void init(); //TODO: = 0
		//TODO: RenderContext should not be & but const&,split RenderContext into changeble part and inchangeble part.
		virtual void execute(RenderContext& ctx) = 0;

		// Resolve framebuffers and shader from the pass description. Called by
		// RenderPipeline before init().
		void configure(const PassDef& def,
		               const std::unordered_map<std::string, Ref<Framebuffer>>& fbos,
		               const Ref<Shader>& shader);

		const std::string& name() const { return m_name; }
		const std::string& type() const { return m_type; }
		const PassDef&     def() const { return m_def; }
		Ref<Shader>        shader() const { return m_shader; }

		// Resolved write target. Null when target is "@default" (see targetsDefault).
		Ref<Framebuffer> target() const { return m_target; }
		bool             targetsDefault() const { return m_targetsDefault; }

		// Ping-pong history, selected by frame parity so it survives across
		// frames. Valid only when the pass declared a history pair.
		bool             hasHistory() const { return (bool)m_history[0]; }
		Ref<Framebuffer> historyWrite(uint32_t frameIndex) const { return m_history[frameIndex & 1]; }
		Ref<Framebuffer> historyRead(uint32_t frameIndex) const { return m_history[(frameIndex & 1) ^ 1]; }

		bool               hasCondition() const { return m_def.hasCondition; }
		const ConditionDef& when() const { return m_def.when; }

		// The framebuffer this pass actually wrote this frame (set in execute()).
		// Lets a history pass report which ping-pong half it used.
		Ref<Framebuffer> written() const { return m_written; }

	protected:
		void setWritten(const Ref<Framebuffer>& fb) { m_written = fb; }

		PassDef          m_def;
		std::string      m_name;
		std::string      m_type;
		Ref<Framebuffer> m_target;
		bool             m_targetsDefault = false;
		Ref<Framebuffer> m_history[2];
		Ref<Shader>      m_shader;
		Ref<Framebuffer> m_written;
	};

}

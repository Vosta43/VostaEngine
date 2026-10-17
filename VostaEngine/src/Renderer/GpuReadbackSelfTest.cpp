#include "vepch.h"
#include "GpuReadbackSelfTest.h"

#include "Core/Log.h"
#include "Renderer/Buffer.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/GpuReadback.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/VertexArray.h"

#include <chrono>
#include <thread>
#include <vector>

namespace ve {

	namespace {

		constexpr uint32_t kCounterTexels = 8;
		constexpr uint32_t kTargetSize = 16;   // 256 fragments, 32 counter bumps each

		const char* kVertexSrc = R"(#version 460 core

layout(location = 0) in vec2 a_Position;

void main()
{
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

		// Each fragment bumps one counter, so a failed image bind or a missing
		// barrier shows up as zeros rather than as a plausible-looking number.
		const char* kFragmentSrc = R"(#version 460 core

layout(location = 0) out vec4 o_Color;
layout(r32ui) uniform uimage2D u_Counter;

void main()
{
    imageAtomicAdd(u_Counter, ivec2(int(gl_FragCoord.x) % 8, 0), 1u);
    o_Color = vec4(0.0, 0.0, 0.0, 1.0);
}
)";
	}

	bool GpuReadbackSelfTest::runOnce() {
		static bool done = false;
		if (done) return false;
		done = true;

		const TextureFormat format = TextureFormat::R32UI;
		const size_t bytes = kCounterTexels * sizeof(uint32_t);

		Ref<Texture2D> counter = Texture2D::create(kCounterTexels, 1, format);
		if (!counter) {
			VE_CORE_ERROR_PRINT("GpuReadbackSelfTest: could not create the counter texture");
			return false;
		}
		std::vector<uint32_t> zeros(kCounterTexels, 0);
		counter->setData(zeros.data(), (uint32_t)bytes);

		Ref<Shader> shader = Shader::create("gpureadback_selftest", kVertexSrc, kFragmentSrc);
		if (!shader || !shader->isValid()) {
			VE_CORE_ERROR_PRINT("GpuReadbackSelfTest: the test shader failed to build");
			return false;
		}

		FramebufferSpec spec;
		spec.width = kTargetSize;
		spec.height = kTargetSize;
		spec.hasDepth = false;
		spec.colorAttachments.resize(1);
		Ref<Framebuffer> target = Framebuffer::create(spec);

		float vertices[] = { -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, 1.0f,  -1.0f, 1.0f };
		uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

		Ref<VertexArray> quad = VertexArray::create();
		auto vb = VertexBuffer::create(vertices, sizeof(vertices));
		vb->setLayout({ { ShaderDataType::Float2, "a_Position" } });
		quad->addVertexBuffer(vb);
		quad->setIndexBuffer(IndexBuffer::create(indices, 6));

		Ref<GpuReadback> readback = GpuReadback::create(bytes);

		target->bind();
		RenderCommand::setViewport(0, 0, kTargetSize, kTargetSize);
		RenderCommand::setDepthTesting(false);
		RenderCommand::setBlend(false);
		shader->bind();
		shader->setImage("u_Counter", counter, 0, ImageAccess::ReadWrite);
		quad->bind();
		RenderCommand::drawIndexed(quad);
		quad->unbind();
		shader->unbind();
		target->unbind();
		RenderCommand::setDepthTesting(true);

		if (!readback->request(counter, 0, 0, kCounterTexels, 1)) {
			VE_CORE_ERROR_PRINT("GpuReadbackSelfTest: the readback request was refused");
			return true;
		}

		// A real consumer comes back on a later frame; this one-shot test sits on
		// the fence instead, so that it can report what came back.
		std::vector<uint32_t> counters(kCounterTexels, 0);
		bool fetched = false;
		for (int i = 0; i < 2000 && !fetched; ++i) {
			fetched = readback->tryFetch(counters.data(), bytes);
			if (!fetched) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		if (!fetched) {
			VE_CORE_ERROR_PRINT("GpuReadbackSelfTest: the fence never signalled");
			return true;
		}

		uint32_t total = 0;
		for (uint32_t c : counters) total += c;

		VE_CORE_SUCCESS_PRINT("GpuReadbackSelfTest: counters=[%u %u %u %u %u %u %u %u] total=%u (expected %u)",
			counters[0], counters[1], counters[2], counters[3],
			counters[4], counters[5], counters[6], counters[7],
			total, kTargetSize * kTargetSize);
		return true;
	}

}

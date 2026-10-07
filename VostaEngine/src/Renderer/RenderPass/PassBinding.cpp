#include "vepch.h"
#include "PassBinding.h"
#include "Core/Log.h"

namespace ve {

	namespace {

		struct Bound {
			enum class Kind { None, Tex2D, Cube, Tex3D };
			Kind              kind = Kind::None;
			Ref<Texture2D>     tex2d;
			Ref<TextureCubeMap> cube;
			Ref<Texture3D>     tex3d;
		};

		Bound fromFramebuffer(const Ref<Framebuffer>& fb, const PassInputDef& in) {
			Bound b;
			if (!fb) return b;
			b.kind = Bound::Kind::Tex2D;
			b.tex2d = in.depth ? fb->getDepthTexture() : fb->getColorTexture((uint32_t)in.attachment);
			return b;
		}

		Bound resolve(const PassInputDef& in, RenderPassBase& pass, RenderContext& ctx) {
			const std::string& s = in.source;

			if (s == "@previous") return fromFramebuffer(ctx.previousTarget, in);
			if (s == "@history")  return fromFramebuffer(pass.historyRead(ctx.frameIndex), in);
			if (s == "@default")  return fromFramebuffer(ctx.outputFrameBuffer, in);

			// Engine texture vocabulary, produced by BakeService each frame.
			Bound b;
			if (s == "@transmittance")      { b.kind = Bound::Kind::Tex2D; b.tex2d = ctx.transmittanceTexture; return b; }
			if (s == "@brdfLUT")            { b.kind = Bound::Kind::Tex2D; b.tex2d = ctx.brdfLUT; return b; }
			if (s == "@weatherMap")         { b.kind = Bound::Kind::Tex2D; b.tex2d = ctx.cloudWeatherMap; return b; }
			if (s == "@scattering")         { b.kind = Bound::Kind::Tex3D; b.tex3d = ctx.scatteringTexture; return b; }
			if (s == "@mieScattering")      { b.kind = Bound::Kind::Tex3D; b.tex3d = ctx.mieScatteringTexture; return b; }
			if (s == "@multipleScattering") { b.kind = Bound::Kind::Tex3D; b.tex3d = ctx.multipleScatteringTexture; return b; }
			if (s == "@cloudNoise")         { b.kind = Bound::Kind::Tex3D; b.tex3d = ctx.cloudNoiseTexture; return b; }
			if (s == "@cloudDetail")        { b.kind = Bound::Kind::Tex3D; b.tex3d = ctx.cloudDetailTexture; return b; }
			if (s == "@cloudWarp")          { b.kind = Bound::Kind::Tex3D; b.tex3d = ctx.cloudWarpTexture; return b; }
			if (s == "@skybox")             { b.kind = Bound::Kind::Cube; b.cube = ctx.skyboxTexture; return b; }
			if (s == "@irradiance")         { b.kind = Bound::Kind::Cube; b.cube = ctx.irradianceMap; return b; }
			if (s == "@prefilteredEnv")     { b.kind = Bound::Kind::Cube; b.cube = ctx.prefilteredEnvMap; return b; }

			// A pass-name match wins over an FBO-name match: a consumer asking for
			// "cloudTaa" wants this frame's accumulated clouds, whichever ping-pong
			// half that pass happened to write.
			if (ctx.passOutputs) {
				auto it = ctx.passOutputs->find(s);
				if (it != ctx.passOutputs->end()) return fromFramebuffer(it->second, in);
			}
			if (ctx.fboRegistry) {
				auto it = ctx.fboRegistry->find(s);
				if (it != ctx.fboRegistry->end()) return fromFramebuffer(it->second, in);
			}

			VE_CORE_WARN_PRINT("PassBinding: pass '%s' input '%s' has unknown source '%s'",
				pass.name().c_str(), in.uniform.c_str(), s.c_str());
			return b;
		}

	} // namespace

	void bindPassInputs(RenderPassBase& pass, RenderContext& ctx) {
		Ref<Shader> sh = pass.shader();
		if (!sh) return;

		for (const auto& in : pass.def().inputs) {
			if (in.uniform.empty() || in.source.empty()) continue;
			Bound b = resolve(in, pass, ctx);
			switch (b.kind) {
				case Bound::Kind::Tex2D: if (b.tex2d) sh->setTexture(in.uniform, b.tex2d, (uint32_t)in.unit); break;
				case Bound::Kind::Cube:  if (b.cube)  sh->setTextureCube(in.uniform, b.cube, (uint32_t)in.unit); break;
				case Bound::Kind::Tex3D: if (b.tex3d) sh->setTexture3D(in.uniform, b.tex3d, (uint32_t)in.unit); break;
				case Bound::Kind::None:  break;
			}
		}
	}

	void applyPassUniforms(RenderPassBase& pass) {
		Ref<Shader> sh = pass.shader();
		if (!sh) return;

		for (const auto& u : pass.def().uniforms) {
			if (u.uniform.empty()) continue;
			switch (u.kind) {
				case UniformKind::Float: sh->setFloat(u.uniform, u.v[0]); break;
				case UniformKind::Int:   sh->setInt(u.uniform, (int)u.v[0]); break;
				case UniformKind::Bool:  sh->setInt(u.uniform, u.vs[0] ? 1 : 0); break;
				case UniformKind::Vec2:  sh->setFloat2(u.uniform, glm::vec2(u.v[0], u.v[1])); break;
				case UniformKind::Vec3:  sh->setFloat3(u.uniform, glm::vec3(u.v[0], u.v[1], u.v[2])); break;
				case UniformKind::Vec4:  sh->setFloat4(u.uniform, glm::vec4(u.v[0], u.v[1], u.v[2], u.v[3])); break;
			}
		}
	}

}

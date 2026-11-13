#include "vepch.h"
#include "PassBinding.h"
#include "Asset/BuiltinReousrces.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/LayeredShaderBuilder.h"
#include "Renderer/RuntimeVirtualTexture.h"
#include "Renderer/Texture.h"
#include "Renderer/Pipeline/SettingsRegistry.h"

namespace ve {

	bool UniformSourceRegistry::apply(const Ref<Shader>& shader, const std::string& uniform,
	                                  const std::string& source, const UniformSourceContext& c) const {
		if (!shader) return false;

		auto it = m_sources.find(source);
		if (it != m_sources.end()) {
			it->second(shader, uniform, c);
			return true;
		}

		if (source.rfind("@settings.", 0) == 0) {
			if (!c.ctx || !c.ctx->settings) return false;
			shader->setFloat(uniform, c.ctx->settings->getFloat(source.substr(10), 0.0f));
			return true;
		}
		return false;
	}

	const UniformSourceRegistry& UniformSourceRegistry::builtin() {
		static const UniformSourceRegistry registry = [] {
			UniformSourceRegistry r;

			r.add("@camera.viewProj", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setMat4(u, c.ctx->projMatrix * c.ctx->viewMatrix);
			});
			r.add("@camera.viewProjNoJitter", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setMat4(u, c.ctx->projMatrixNoJitter * c.ctx->viewMatrix);
			});
			r.add("@camera.view", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setMat4(u, c.ctx->viewMatrix);
			});
			r.add("@camera.proj", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setMat4(u, c.ctx->projMatrix);
			});
			r.add("@camera.position", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setFloat3(u, toRenderSpaceChart(c.ctx->cameraPosition, *c.ctx));
			});
			r.add("@world.origin", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setFloat3(u, glm::vec3(c.ctx->renderOrigin));
			});
			r.add("@time.total", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setFloat(u, c.ctx->totalTime);
			});
			r.add("@time.delta", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setFloat(u, c.ctx->deltaTime);
			});
			r.add("@frame.index", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				s->setInt(u, (int)c.ctx->frameIndex);
			});
			r.add("@mesh.model", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				if (c.model) s->setMat4(u, *c.model);
			});
			r.add("@mesh.transform", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				if (c.mesh) s->setMat4(u, c.mesh->transform);
			});
			r.add("@mesh.position", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				if (c.mesh) s->setFloat3(u, toRenderSpaceChart(c.mesh->worldPosition, *c.ctx));
			});
			r.add("@sprite.model", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				if (c.model) s->setMat4(u, *c.model);
			});
			r.add("@sprite.size", [](const Ref<Shader>& s, const std::string& u, const UniformSourceContext& c) {
				if (c.sprite) s->setFloat2(u, c.sprite->size);
			});
			return r;
		}();
		return registry;
	}

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
		bindPassInputs(pass, ctx, pass.shader());
	}

	void bindPassInputs(RenderPassBase& pass, RenderContext& ctx, const Ref<Shader>& sh) {
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
		applyPassUniforms(pass, pass.shader());
	}

	void applyPassUniforms(RenderPassBase& pass, const Ref<Shader>& sh) {
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

	void applyMaterialBindings(const Ref<Shader>& shader, const MaterialBindingSet& bindings) {
		if (!shader) return;

		for (const auto& t : bindings.textures) {
			if (t.target == TextureTarget::Texture2DArray) {
				auto tex = ResourceManager::get<Texture2DArray>(t.textureHandle);
				if (tex) shader->setTextureArray(t.uniformName, tex, t.unit);
			}
			else {
				auto tex = ResourceManager::get<Texture2D>(t.textureHandle);
				if (tex) shader->setTexture(t.uniformName, tex, t.unit);
			}
		}
		for (const auto& b : bindings.ints)   shader->setInt(b.name, b.value);
		for (const auto& b : bindings.floats) shader->setFloat(b.name, b.value);
		for (const auto& b : bindings.vec2s)  shader->setFloat2(b.name, b.value);
		for (const auto& b : bindings.vec3s)  shader->setFloat3(b.name, b.value);
	}

	void drawGlobeTiles(RenderContext& ctx, MaterialBindingSet& scratch) {
		// Sphere mode needs both the material and its baked atlas; the terrain's
		// layered material gives the globe the same surface rules as the terrain.
		// Without both, the default material still shapes a readable planet.
		const bool sphereMode = ctx.globeMaterial != nullptr && ctx.globeWeights != nullptr;
		Ref<Material> material = sphereMode
			? ctx.globeMaterial
			: ResourceManager::get<Material>(BuiltinResources::getDefaultMaterial());
		if (!material) return;

		Ref<Shader> shader = material->getShader();
		if (!shader || !shader->isValid()) return;

		shader->bind();
		shader->setMat4("u_ViewProj", ctx.projMatrix * ctx.viewMatrix);
		shader->setFloat("u_Time", ctx.totalTime);
		shader->setFloat3("u_WorldOrigin", glm::vec3(ctx.renderOrigin));
		shader->setInt("u_CoordMode", sphereMode ? 1 : 0);

		material->fillBindings(scratch);
		applyMaterialBindings(shader, scratch);

		// Replace the material's own weight array with the direction-domain
		// atlas: same unit the bindings just used, so the rebind wins.
		if (sphereMode) {
			ctx.globeWeights->bind(LayeredShaderBuilder::kWeightsTextureUnit);
			shader->setInt("u_Weights", (int)LayeredShaderBuilder::kWeightsTextureUnit);
		}

		for (const auto& tile : ctx.globeTiles) {
			if (!tile.mesh) continue;
			shader->setMat4("u_Model", glm::translate(glm::mat4(1.0f),
				toRenderSpace(ctx.planetCenter + tile.centerLocal, ctx.renderOrigin)));
			tile.mesh->bind();
			tile.mesh->draw(0, 0);
			tile.mesh->unbind();
		}

		shader->unbind();
	}

	void bindRuntimeVirtualTexture(const Ref<Shader>& shader, const RuntimeVirtualTexture& rvt) {
		if (!shader) return;

		shader->setFloat2("u_RVTWorldMin", rvt.worldMin());
		shader->setFloat2("u_RVTWorldSize", rvt.worldSize());
		shader->setInt("u_PagesX", rvt.pagesX());
		shader->setInt("u_PagesY", rvt.pagesY());
		shader->setInt("u_MipCount", rvt.mipCount());
		shader->setInt("u_RVTPageSize", rvt.pageSizeTexels());
		shader->setInt("u_RVTBorder", rvt.borderTexels());

		shader->setTexture("u_PageTable", rvt.pageTable(), RuntimeVirtualTexture::kPageTableUnit);
		for (int i = 0; i < 4; ++i) {
			if (auto arr = rvt.physical(i))
				shader->setTextureArray("u_Physical" + std::to_string(i), arr,
				                        RuntimeVirtualTexture::kPhysicalUnit + (uint32_t)i);
		}
	}

}

#include "vepch.h"
#include "LayeredShaderBuilder.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/MaterialLayer.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/ShaderGenerator.h"
#include "Renderer/SingleMaterial.h"

namespace ve {

	namespace {

		constexpr const char* kTemplatePath = "VostaEngine/resources/shaders/terrain_layers.template.glsl";

		constexpr const char* kSamplerDeclsMarker  = "/*__SAMPLER_DECLS__*/";
		constexpr const char* kEvalFuncsMarker     = "/*__EVAL_FUNCS__*/";
		constexpr const char* kHeightPrepassMarker = "/*__HEIGHT_PREPASS__*/";
		constexpr const char* kLayerBlendMarker    = "/*__LAYER_BLEND__*/";
		constexpr const char* kMaxLayersMarker     = "/*__MAX_LAYERS_DECL__*/";

		// One sampler a layer wants bound, in the order it was declared.
		struct SamplerSlot {
			std::string name;
			AssetHandle handle;
		};

		std::string floatStr(float v) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.6g", (double)v);
			return buf;
		}

		std::string vec3Str(const glm::vec3& v) {
			char buf[96];
			std::snprintf(buf, sizeof(buf), "vec3(%.6g, %.6g, %.6g)",
			              (double)v.x, (double)v.y, (double)v.z);
			return buf;
		}

		bool replaceAll(std::string& s, const std::string& from, const std::string& to) {
			if (from.empty()) return false;
			bool found = false;
			for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) {
				s.replace(pos, from.size(), to);
				found = true;
			}
			return found;
		}

		std::string readFile(const std::string& path, bool& ok) {
			std::ifstream in(path, std::ios::binary);
			if (!in) { ok = false; return {}; }
			std::stringstream ss;
			ss << in.rdbuf();
			ok = true;
			return ss.str();
		}

		// The signature every layer function shares. The parameter names match what a
		// material graph's codegen emits (v_TexCoord / v_WorldPos / v_TBN), so a
		// spliced graph body resolves against them; the normal comes out in world space.
		std::string evalFunc(int i, const std::string& body) {
			std::string s;
			s += "void evalLayer_" + std::to_string(i) + "(\n";
			s += "    vec2 v_TexCoord, vec3 v_WorldPos, mat3 v_TBN,\n";
			s += "    out vec3 o_albedo, out vec3 o_worldN, out float o_rough,\n";
			s += "    out float o_metal, out float o_ao, out vec3 o_emis)\n{\n";
			s += body;
			s += "}\n\n";
			return s;
		}

		// The six output writes, shared by the graph path and the fixed-slot path.
		// The normal is tangent-space on the way in and world-space on the way out.
		std::string outputWrites(const SurfaceOutputs& o) {
			return
				"    o_albedo = " + o.exprs[0] + ";\n"
				"    o_metal  = " + o.exprs[1] + ";\n"
				"    o_rough  = " + o.exprs[2] + ";\n"
				"    o_ao     = " + o.exprs[3] + ";\n"
				"    o_worldN = normalize(v_TBN * normalize(" + o.exprs[4] + " * 2.0 - 1.0));\n"
				"    o_emis   = " + o.exprs[5] + ";\n";
		}

		// White albedo, flat normal, no metallic, roughness 0.5, full AO, no
		// emissive: what a layer shows when its material is missing or unusable.
		SurfaceOutputs fallbackSurface() {
			SurfaceOutputs o;
			o.exprs[0] = "vec3(1.0)";
			o.exprs[1] = "0.0";
			o.exprs[2] = "0.5";
			o.exprs[3] = "1.0";
			o.exprs[4] = "vec3(0.5, 0.5, 1.0)";
			o.exprs[5] = "vec3(0.0)";
			return o;
		}

		// A material with no graph: sample its fixed PBR slots, falling back to the
		// scalar params where a slot is unassigned. Declares one sampler per assigned
		// map and returns the six expressions that read them.
		SurfaceOutputs fixedSlotSurface(const SingleMaterial& m, int layerIndex,
		                                std::string& decls, std::vector<SamplerSlot>& slots) {
			SurfaceOutputs o;
			const std::string suffix = "_L" + std::to_string(layerIndex);

			auto addSampler = [&](const char* base, AssetHandle h) {
				const std::string name = std::string(base) + suffix;
				decls += "uniform sampler2D " + name + ";\n";
				slots.push_back({name, h});
				return name;
			};

			if (m.albedoMapHandle.isValid())
				o.exprs[0] = "texture(" + addSampler("u_LayerBase", m.albedoMapHandle) + ", v_TexCoord).rgb";
			else
				o.exprs[0] = vec3Str(m.albedoColor);

			if (m.metallicMapHandle.isValid())
				o.exprs[1] = "texture(" + addSampler("u_LayerMetal", m.metallicMapHandle) + ", v_TexCoord).r";
			else
				o.exprs[1] = floatStr(m.metallic);

			if (m.roughnessMapHandle.isValid())
				o.exprs[2] = "texture(" + addSampler("u_LayerRough", m.roughnessMapHandle) + ", v_TexCoord).r";
			else
				o.exprs[2] = floatStr(m.roughness);

			if (m.aoMapHandle.isValid())
				o.exprs[3] = "texture(" + addSampler("u_LayerAO", m.aoMapHandle) + ", v_TexCoord).r";
			else
				o.exprs[3] = floatStr(m.ao);

			if (m.normalMapHandle.isValid())
				o.exprs[4] = "texture(" + addSampler("u_LayerNrm", m.normalMapHandle) + ", v_TexCoord).rgb";
			else
				o.exprs[4] = "vec3(0.5, 0.5, 1.0)";

			if (m.emissiveMapHandle.isValid())
				o.exprs[5] = "texture(" + addSampler("u_LayerEmis", m.emissiveMapHandle) + ", v_TexCoord).rgb";
			else
				o.exprs[5] = vec3Str(m.emissiveColor);

			return o;
		}

	} // namespace

	LayeredBuildResult LayeredShaderBuilder::build(const LayeredMaterial& mat) {
		LayeredBuildResult r;

		bool readOk = false;
		const std::string tmpl = readFile(toAbsolute(kTemplatePath), readOk);
		if (!readOk) {
			r.error = std::string("cannot read shader template ") + kTemplatePath;
			return r;
		}

		const auto vpos = tmpl.find("#shader vertex");
		const auto fpos = tmpl.find("#shader fragment");
		if (vpos == std::string::npos || fpos == std::string::npos) {
			r.error = "shader template missing #shader vertex / #shader fragment";
			return r;
		}

		r.vertSrc = tmpl.substr(vpos + 15, fpos - vpos - 15);
		std::string fs = tmpl.substr(fpos + 17);

		const int count = (int)std::min(mat.layers.size(), (size_t)kMaxMaterialLayers);
		// The shader's arrays and weight loops are sized to this material, not the global
		// ceiling, so a two-layer material compiles a two-slice shader.
		const int maxLayers = std::max(1, count);

		std::stringstream samplerDecls, evalFuncs, heightPrepass, layerBlend;
		std::vector<SamplerSlot> contentSamplers;   // allocation order == unit order

		// Resolve every layer asset once; the Refs keep them alive for the whole call.
		std::vector<Ref<MaterialLayerAsset>> layerAssets(count);
		for (int i = 0; i < count; ++i)
			layerAssets[i] = ResourceManager::get<MaterialLayerAsset>(mat.layers[i]);

		auto allocSampler = [&](const std::string& name, AssetHandle h) {
			contentSamplers.push_back({name, h});
			return (int)contentSamplers.size();
		};
		auto overflowError = [&](int layerIndex) {
			return "layer " + std::to_string(layerIndex) + " pushes the content samplers past "
				+ std::to_string(kMaxContentSamplers) + " (fragment-unit budget)";
		};

		for (int i = 0; i < count; ++i) {
			const MaterialLayer* layer = layerAssets[i] ? &layerAssets[i]->layer : nullptr;

			auto sm = layer ? std::dynamic_pointer_cast<SingleMaterial>(
			                      ResourceManager::get<Material>(layer->material))
			                : nullptr;

			// ── surface: inline the material's graph, else its fixed slots ──────
			std::string decls, body;
			std::vector<SamplerSlot> slots;
			bool done = false;

			if (sm && sm->graph.nodes.size() > 1) {
				std::string gdecls, gbody;
				SurfaceOutputs g;
				if (ShaderGenerator::compileSurface(
						sm->graph, "u_MatTex_L" + std::to_string(i) + "_", gdecls, gbody, g)) {
					decls = std::move(gdecls);
					for (auto& s : g.samplers) slots.push_back({s.first, s.second});
					body = gbody + outputWrites(g);
					done = true;
				}
			}

			if (!done) {
				if (sm) {
					SurfaceOutputs s = fixedSlotSurface(*sm, i, decls, slots);
					body = outputWrites(s);
				} else {
					if (layer && layer->material.isValid())
						VE_CORE_WARN_PRINT("LayeredShaderBuilder: layer %d references a non-single "
						                   "material; using the default surface", i);
					body = outputWrites(fallbackSurface());
				}
			}

			for (auto& s : slots) {
				if (allocSampler(s.name, s.handle) > kMaxContentSamplers) {
					r.error = overflowError(i);
					return r;
				}
			}
			samplerDecls << decls;
			evalFuncs << evalFunc(i, body);

			// Layer UV: world XZ / tiling on the chart, a cube-face UV on the globe (no
			// world scale there -- the tile mesh UV has none either, and a cube face
			// keeps the poles from collapsing the texture). The chart branch spells the
			// absolute XZ out rather than reusing the template's worldXZ, which is
			// declared after this block is spliced.
			const std::string layerUV =
				"((u_CoordMode == 1) ? (veCubeUV(geomNormal) * u_GlobeTiles)"
				" : ((v_WorldPosition.xz + u_WorldOrigin.xz) / max(u_LayerTiling[" + std::to_string(i) + "], 1e-4)))";

			// ── height (layer-local; drives HeightBlend) ────────────────────────
			if (layer && layer->heightMap.isValid()) {
				const std::string hn = "u_LayerHeight_" + std::to_string(i);
				if (allocSampler(hn, layer->heightMap) > kMaxContentSamplers) {
					r.error = overflowError(i);
					return r;
				}
				samplerDecls << "uniform sampler2D " << hn << ";\n";
				heightPrepass << "    hL[" << i << "] = texture(" << hn << ", " << layerUV << ").r;\n";
			} else {
				heightPrepass << "    hL[" << i << "] = 1.0;\n";
			}

			// ── eval call ───────────────────────────────────────────────────────
			// The graph sees absolute world position: add the render origin back to the
			// camera-relative varying so a layer's textures stay pinned to the world.
			layerBlend << "    evalLayer_" << i
			           << "(" << layerUV << ", "
			              "(v_WorldPosition.xyz + u_WorldOrigin), TBN,\n"
			           << "                sAlb[" << i << "], sNrm[" << i << "], sRgh[" << i
			           << "], sMet[" << i << "], sAoo[" << i << "], sEmi[" << i << "]);\n";
		}

		// hL is sized MAX_LAYERS; keep the single slot a zero-layer material still
		// declares initialized so nothing reads an uninitialized array element.
		for (int i = count; i < maxLayers; ++i)
			heightPrepass << "    hL[" << i << "] = 1.0;\n";

		if (!replaceAll(fs, kSamplerDeclsMarker, samplerDecls.str()) ||
		    !replaceAll(fs, kEvalFuncsMarker, evalFuncs.str()) ||
		    !replaceAll(fs, kHeightPrepassMarker, heightPrepass.str()) ||
		    !replaceAll(fs, kLayerBlendMarker, layerBlend.str()) ||
		    !replaceAll(fs, kMaxLayersMarker,
		        "const int MAX_LAYERS = " + std::to_string(maxLayers) + ";")) {
			r.error = "shader template missing one or more insertion markers";
			return r;
		}

		r.fragSrc = std::move(fs);
		r.contentSamplerCount = (int)contentSamplers.size();
		for (int i = 0; i < (int)contentSamplers.size(); ++i)
			r.textures.push_back({contentSamplers[i].name, contentSamplers[i].handle, (uint32_t)i});

		r.ok = true;
		return r;
	}

}

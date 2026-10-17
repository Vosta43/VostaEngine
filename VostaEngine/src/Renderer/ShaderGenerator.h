#pragma once

#include "MaterialGraph.h"
#include "Core/AssetHandle.h"

#include <array>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

namespace ve {

	struct ShaderGenContext {
		std::stringstream uniforms;
		std::stringstream samplers;
		std::stringstream inputs;
		std::stringstream outputs;
		std::stringstream functions;
		std::stringstream mainFunction;

		int currentVarNum = 0;

		std::string allocVar(const std::string& prefix = "n") {
			return prefix + std::to_string(currentVarNum++);
		}
	};

	// One material graph evaluated as a surface: the six resolved MaterialOutput
	// expressions (no DEFAULT_ sentinel — unconnected pins carry the real default)
	// plus the texture sampler each TextureSampler node wants bound.
	struct SurfaceOutputs {
		// BaseColor, Metallic, Roughness, AO, Normal (tangent space), Emissive.
		std::array<std::string, 6> exprs;
		std::vector<std::pair<std::string, AssetHandle>> samplers;
	};

	class ShaderGenerator {
	public:

		static void compile(const MaterialGraph& graph,std::string& outVertSrc, std::string& outFragSrc);

		// Evaluate only the surface of a material graph into a self-contained GLSL
		// body: the node statements plus the six resolved output expressions. Unlike
		// compile() it never touches Frag0..3 or the hardcoded baseColor/metallic/...
		// locals, so its body can be spliced into a per-layer function that a caller
		// composes and blends. Sampler uniforms are named <samplerPrefix><slot> (each
		// call gets its own slot counter, so callers must pass a distinct prefix per
		// layer). Returns false when the graph has no MaterialOutput node.
		static bool compileSurface(const MaterialGraph& graph,
		                           const std::string& samplerPrefix,
		                           std::string& outSamplerDecls,
		                           std::string& outBody,
		                           SurfaceOutputs& out);

	private:

		static void generateNodeCode(Ref<GraphNode> node,const MaterialGraph& graph,ShaderGenContext& ctx,std::unordered_map<uint32_t, std::vector<std::string>>& varCache);

	};

}
#pragma once

#include "MaterialGraph.h"

#include <sstream>
#include <string>
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

	class ShaderGenerator {
	public:
		
		static void compile(const MaterialGraph& graph,std::string& outVertSrc, std::string& outFragSrc);

	private:
		
		static void generateNodeCode(Ref<GraphNode> node,const MaterialGraph& graph,ShaderGenContext& ctx,std::unordered_map<uint32_t, std::vector<std::string>>& varCache);
		
	};

}
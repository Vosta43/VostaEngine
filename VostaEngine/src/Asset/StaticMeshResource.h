#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"

#include <glm.hpp>
#include <vector>

namespace ve {

	struct Vertex {
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
		glm::vec3 tangent;
	};
	//
	struct VE_API SubMeshResource {

		std::string name; //For display
		uint32_t firstIndex;
		uint32_t indexCount; // Continous count. For renderer.
		AssetHandle materialHandle;

	};
	// TODO: Devided into submeshes
	struct VE_API StaticMeshResource {
		std::vector<Vertex> vertexBuffer;
		std::vector<int> indexBuffer;

		std::vector<SubMeshResource> subMeshes;
	};

}

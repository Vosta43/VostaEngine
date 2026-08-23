#pragma once

#include "Renderer/StaticMesh.h"
#include "Renderer/Texture.h"
#include "Asset/TextureResource.h"

namespace ve {

	class VE_API TerrainMeshBuilder {
	public:

		static Ref<StaticMesh> buildMeshFromHeightMap(const Ref<TextureResource>& heightMap,float tileSize = 1.0f,float heightScale = 1.0f);
		// Build meshes from height map (blocks stands for 1x1 2x2 etc.)
		static std::vector<Ref<StaticMesh>> buildMeshesFromHeightMap(const Ref<TextureResource>& heightMap, float tileSize = 1.0f, float heightScale = 1.0f,int blocks = 1);
	private:

	};

}

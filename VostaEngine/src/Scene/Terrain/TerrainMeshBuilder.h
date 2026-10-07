#pragma once

#include "Renderer/StaticMesh.h"
#include "Renderer/Texture.h"
#include "Asset/TextureResource.h"

namespace ve {

	class VE_API TerrainMeshBuilder {
	public:

		static Ref<StaticMesh> buildMeshFromHeightMap(const Ref<TextureResource>& heightMap,float tileSize = 1.0f,float heightScale = 1.0f);
		// Same vertex math as buildMeshFromHeightMap, but returns the raw resource
		// (vertices only) so a caller like QuadTreeTerrain can keep the shared
		// full-res vertex buffer while supplying its own index buffer.
		static Ref<StaticMeshResource> buildMeshResourceFromHeightMap(const Ref<TextureResource>& heightMap, float tileSize = 1.0f, float heightScale = 1.0f);
		// Build meshes from height map (blocks stands for 1x1 2x2 etc.)
		static std::vector<Ref<StaticMesh>> buildMeshesFromHeightMap(const Ref<TextureResource>& heightMap, float tileSize = 1.0f, float heightScale = 1.0f,int blocks = 1);
	private:

	};

}

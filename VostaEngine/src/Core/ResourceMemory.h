#pragma once

#include <cstddef>

namespace ve {

	class Texture2D;
	class Texture2DArray;
	class TextureCubeMap;
	class StaticMesh;
	class Material;
	struct StaticMeshResource;
	struct NoiseGraphResource;
	struct GeometryGraphResource;
	struct TerrainRuleGraphResource;
	struct MaterialLayerAsset;
	struct TerrainDataResource;
	class HudAsset;

	// Approximate resident cost of one resource, in bytes. Only drives eviction
	// pressure and the stats view, so counts x stride is enough: no walking of
	// allocators, no per-platform split.
	size_t estimateBytes(const Texture2D& texture);
	size_t estimateBytes(const Texture2DArray& texture);
	size_t estimateBytes(const TextureCubeMap& texture);
	size_t estimateBytes(const StaticMesh& mesh);
	size_t estimateBytes(const Material& material);
	size_t estimateBytes(const StaticMeshResource& mesh);
	size_t estimateBytes(const NoiseGraphResource& resource);
	size_t estimateBytes(const GeometryGraphResource& resource);
	size_t estimateBytes(const TerrainRuleGraphResource& resource);
	size_t estimateBytes(const MaterialLayerAsset& layer);
	size_t estimateBytes(const TerrainDataResource& resource);
	size_t estimateBytes(const HudAsset& hud);
}

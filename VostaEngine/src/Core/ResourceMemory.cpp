#include "vepch.h"
#include "Core/ResourceMemory.h"

#include "Renderer/Texture.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Material.h"
#include "Asset/StaticMeshResource.h"
#include "Noise/NoiseGraphResource.h"
#include "Geometry/GeometryGraphResource.h"
#include "Terrain/RuleGraphResource.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Scene/Terrain/TerrainDataResource.h"
#include "Gui/Hud/HudAsset.h"

#include <cstdint>

namespace ve {

	namespace {

		size_t texelBytes(TextureFormat format) {
			switch (format) {
				case TextureFormat::R8:           return 1;
				case TextureFormat::R16F:         return 2;
				case TextureFormat::R16UI:        return 2;
				case TextureFormat::RGB:          return 3;
				case TextureFormat::SRGB8:        return 3;
				case TextureFormat::RGBA:         return 4;
				case TextureFormat::SRGB8_ALPHA8: return 4;
				case TextureFormat::RG16F:        return 4;
				case TextureFormat::R32UI:        return 4;
				case TextureFormat::RGB16F:       return 6;
				case TextureFormat::RGBA16F:      return 8;
				default:                          return 4;   // NONE: assume RGBA8
			}
		}

		constexpr size_t kGraphNodeBytes = 256;
		constexpr size_t kMaterialBytes  = 8 * 1024;
		constexpr size_t kLayerBytes     = 4 * 1024;
		constexpr size_t kHudBytes       = 4 * 1024;
	}

	size_t estimateBytes(const Texture2D& texture) {
		// File-loaded textures allocate the whole mip chain.
		const size_t level0 = (size_t)texture.getWidth() * texture.getHeight()
			* texelBytes(texture.getFormat());
		return level0 * 4 / 3;
	}

	size_t estimateBytes(const Texture2DArray& texture) {
		// Arrays are allocated single-level.
		return (size_t)texture.getWidth() * texture.getHeight() * texture.getLayers()
			* texelBytes(texture.getFormat());
	}

	size_t estimateBytes(const TextureCubeMap& texture) {
		// A stored cube map is file-loaded: width is the 4-face-wide cross image.
		// Single-level storage, no mip factor.
		const size_t face = (size_t)texture.getWidth() / 4;
		return face * face * 6 * texelBytes(texture.getFormat());
	}

	size_t estimateBytes(const StaticMesh& mesh) {
		return (size_t)mesh.getVertexCount() * sizeof(Vertex)
			+ (size_t)mesh.getIndexCount() * sizeof(uint32_t);
	}

	size_t estimateBytes(const StaticMeshResource& mesh) {
		return mesh.vertexBuffer.size() * sizeof(Vertex)
			+ mesh.indexBuffer.size() * sizeof(int)
			+ mesh.subMeshes.size() * sizeof(SubMeshResource);
	}

	size_t estimateBytes(const Material&) {
		// Compiled shaders live outside this interface; a flat allowance keeps a
		// material from being treated as free.
		return kMaterialBytes;
	}

	size_t estimateBytes(const NoiseGraphResource& resource) {
		return resource.graph.nodes.size() * kGraphNodeBytes
			+ resource.graph.links.size() * 32;
	}

	size_t estimateBytes(const GeometryGraphResource& resource) {
		return resource.graph.nodes.size() * kGraphNodeBytes
			+ resource.graph.links.size() * 32;
	}

	size_t estimateBytes(const TerrainRuleGraphResource& resource) {
		return resource.graph.nodes.size() * kGraphNodeBytes
			+ resource.graph.links.size() * 32;
	}

	size_t estimateBytes(const MaterialLayerAsset&) {
		return kLayerBytes;
	}

	size_t estimateBytes(const TerrainDataResource& resource) {
		size_t bytes = 0;
		for (const auto& [key, tile] : resource.tiles)
			bytes += tile.weightData.size() + sizeof(TerrainDataTile);
		return bytes;
	}

	size_t estimateBytes(const HudAsset&) {
		return kHudBytes;
	}
}

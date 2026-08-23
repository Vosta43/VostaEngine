#pragma once

#include "Core/Log.h"
#include "Material.h"
#include "Core/AssetHandle.h"
#include "Texture.h"
#include "RenderPipeline.h"

namespace ve {
	
	class VE_API ThumbnailRenderer {
	public:
		
		static Ref<Texture2D> getMaterialThumbnail(AssetHandle materialHandle);
		static Ref<Texture2D> getStaticMeshThumbnail(AssetHandle staticMeshHandle);

		static void invalidate(AssetHandle materialHandle);
		static void invalidateMesh(AssetHandle staticMeshHandle);


	private:
		struct CachedThumbnail {
			Ref<Texture2D> texture;
		};
		static std::unordered_map<uint32_t, CachedThumbnail> s_cache;
		static std::unordered_map<uint32_t, CachedThumbnail> s_meshCache;

	};

}
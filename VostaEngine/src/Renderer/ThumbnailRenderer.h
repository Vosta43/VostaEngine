#pragma once

#include "Core/Log.h"
#include "Material.h"
#include "Core/AssetHandle.h"
#include "Texture.h"
#include "RenderPipeline.h"
#include "FrameBuffer.h"

namespace ve {
	
	class VE_API ThumbnailRenderer {
	public:
		
		static Ref<Texture2D> getMaterialThumbnail(AssetHandle materialHandle);
		static Ref<Texture2D> getStaticMeshThumbnail(AssetHandle staticMeshHandle);

		// Single proactive bake entry: bakes previews for every material and
		// static mesh currently loaded. Cached entries are skipped, so repeated
		// calls are cheap. Call it wherever the loaded asset set changes (scene
		// load, import); the get*Thumbnail() accessors still bake lazily on a
		// cache miss for assets that appear later.
		static void bakeAllLoaded();

		static void invalidate(AssetHandle materialHandle);
		static void invalidateMesh(AssetHandle staticMeshHandle);

		// Fills `ctx` with the IBL and atmosphere inputs the scene view uses (via
		// BakeService), so an offscreen preview renders with real lighting and no
		// scene loaded. Shared by the thumbnail bakes and the editor's live mesh
		// previews.
		static void applyPreviewLighting(RenderContext& ctx);


	private:
		struct CachedThumbnail {
			Ref<Texture2D> texture;
			// The FBO owns the GL name behind `texture` (a non-owning wrapper)
			// and deletes it on destruction, so the cache has to keep it alive.
			Ref<Framebuffer> owner;
		};
		static std::unordered_map<uint32_t, CachedThumbnail> s_cache;
		static std::unordered_map<uint32_t, CachedThumbnail> s_meshCache;

	};

}
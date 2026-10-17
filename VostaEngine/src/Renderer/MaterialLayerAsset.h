#pragma once

#include "Core/Core.h"
#include "Renderer/MaterialLayer.h"
#include "Scene/Archive.h"

namespace ve {

	// A surface layer as a standalone asset: the texture source plus the rule
	// deciding where it shows up. LayeredMaterial references these by handle, so
	// one layer can be shared across materials and edited in its own window.
	struct VE_API MaterialLayerAsset {
		// On-disk kind tag, the first token of the .veasset file.
		static constexpr const char* kTypeTag = "material_layer";
		// Bumped whenever the body layout changes; a mismatch discards the file
		// rather than misreading it. 3 = a SingleMaterial reference plus a height
		// path (v2 stored four separate texture paths).
		static constexpr int32_t kFormatVersion = 3;

		MaterialLayer layer;

		// Factory for ResourceManager::store<MaterialLayerAsset>.
		static Ref<MaterialLayerAsset> create(const std::string& path);

		void serialize(Archive& ar) const;
		void deserialize(Archive& ar);

		// Writes a fresh single-layer .veasset to `path` (adding the extension if
		// missing). The caller registers it via ResourceManager::store.
		static void writeNewAsset(const std::string& path);
	};

}

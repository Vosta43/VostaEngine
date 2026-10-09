#pragma once

#include "Core/Core.h"
#include "Renderer/MaterialLayer.h"
#include "Scene/Archive.h"

#include <array>

namespace ve {

	// A surface layer as a standalone asset: the logical texture maps plus the
	// rule deciding where it shows up. LayeredMaterial references these by handle,
	// so one layer can be shared across materials and edited in its own window.
	struct VE_API MaterialLayerAsset {
		// On-disk kind tag, the first token of the .veasset file.
		static constexpr const char* kTypeTag = "material_layer";
		// Bumped whenever the body layout changes; a mismatch discards the file
		// rather than misreading it. 2 = separate albedo/normal/roughness/height
		// paths (v1 stored two pre-packed textures).
		static constexpr int32_t kFormatVersion = 2;

		MaterialLayer layer;

		// The two channel-packed GPU textures the shader samples, built from the
		// logical maps on first use:
		//   albedoHeight:   rgb = albedo,  a = height
		//   normalRoughness: rgb = normal, a = roughness
		// Unset maps become white / flat-normal / 1.0. Rebuilt automatically when
		// any source slot changes.
		struct PackedTextures {
			AssetHandle albedoHeight;
			AssetHandle normalRoughness;
		};
		const PackedTextures& packedTextures() const;

		// Factory for ResourceManager::store<MaterialLayerAsset>.
		static Ref<MaterialLayerAsset> create(const std::string& path);

		void serialize(Archive& ar) const;
		void deserialize(Archive& ar);

		// Writes a fresh single-layer .veasset to `path` (adding the extension if
		// missing). The caller registers it via ResourceManager::store.
		static void writeNewAsset(const std::string& path);

	private:
		// Cache of the pack above, guarded by the source handles it was built from.
		mutable PackedTextures m_packed;
		mutable std::array<AssetHandle, 4> m_packKey;
		mutable bool m_packValid = false;
	};

}

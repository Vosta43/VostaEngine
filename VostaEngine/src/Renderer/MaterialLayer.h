#pragma once

#include "Core/AssetHandle.h"

#include <string>

namespace ve {

	// How a layer's weight joins the blend.
	enum class MaterialLayerBlend {
		WeightBlend,  // normalized against the other weight-blend layers
		AlphaBlend,   // used raw, composited on top of the accumulated result
		HeightBlend,  // modulated by the layer's height channel
	};

	// Highest layer count the layered surface shader is compiled for. Every layer
	// costs two samplers (albedo+height, normal+roughness), so this must stay within
	// the fixed sampler array the shader declares.
	inline constexpr int kMaxMaterialLayers = 4;

	// One surface layer: a set of logical texture maps plus the rule deciding
	// where it shows up. Nothing here is tied to a particular surface kind — the
	// shader turns these into per-layer blend weights.
	//
	// The maps are authored as separate images and may each be left unset. The
	// runtime packs them into the two channel-packed samplers the shader samples
	// (see MaterialLayerAsset::packedTextures) — albedo rgb + height a, normal
	// rgb + roughness a — so authoring never needs a manual pack step.
	struct MaterialLayer {

		// Identity. Not required to be unique.
		std::string name = "Layer";

		AssetHandle albedoMap;     // rgb
		AssetHandle normalMap;     // rgb (tangent space)
		AssetHandle roughnessMap;  // red channel used
		AssetHandle heightMap;     // red channel used; drives HeightBlend

		// World-space UV scale: uv = worldXZ / tiling.
		float tiling = 8.0f;

		// Procedural weight rule: the weight is the product of the enabled terms,
		// then normalized against the other weight-blend layers. Slope is
		// 1 - worldNormal.y (0 = flat, 1 = vertical); height is the world Y
		// expressed as a fraction of the surface's heightScale.
		bool  useSlope = true;
		float slopeMin = 0.0f;
		float slopeMax = 0.6f;

		bool  useHeight = false;
		float heightMin = 0.0f;
		float heightMax = 1.0f;

		// Breaks up the rule's boundary so the transition is not a clean line.
		// 0 = rule only.
		float noiseStrength = 0.5f;
		float noiseScale    = 40.0f;

		MaterialLayerBlend blend = MaterialLayerBlend::WeightBlend;
	};

}

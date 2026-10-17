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

	// Ceiling on a layered material's layers. Weights live in a texture array (one
// slice per layer) and the shader is generated per material with MAX_LAYERS set to
// that material's own layer count, so a material's arrays and weight slices stay as
// small as it needs. What caps the count is samplers: each layer costs one or more
// content samplers (its material graph's samplers, or its fixed-slot maps) and the
// weight array takes one, sharing GL's 16 guaranteed fragment texture units. Raise
// this together with kWeightsTextureUnit / kMaxContentSamplers if the target GPUs
// expose more units (desktop NVIDIA/Intel report 32, which would allow 31 layers).
	inline constexpr int kMaxMaterialLayers = 15;

	// One surface layer: a material reference plus the rule deciding where it shows
	// up. Nothing here is tied to a particular surface kind — the shader turns these
	// into per-layer blend weights.
	//
	// Albedo, normal, roughness, metallic, AO and emissive are evaluated from the
	// referenced material (its node graph, or its fixed PBR slots) once per layer;
	// only height is layer-local, since SingleMaterial has no height channel.
	struct MaterialLayer {

		// Identity. Not required to be unique.
		std::string name = "Layer";

		// Surface source: a SingleMaterial, evaluated per layer. Unset falls back to
		// white albedo, flat normal, roughness 0.5 and no metallic/emissive.
		AssetHandle material;
		AssetHandle heightMap;     // red channel used; drives HeightBlend

		// World-space UV scale: uv = worldXZ / tiling.
		float tiling = 8.0f;

		// Procedural weight rule, evaluated on the CPU into the terrain's weight map
		// (not in the shader): the weight is the product of the enabled terms. Slope
		// is 1 - worldNormal.y (0 = flat, 1 = vertical); height is the world Y
		// expressed as a fraction of the surface's heightScale. Editing these re-bakes
		// the weights, overwriting painted coverage the same way a rule graph does.
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

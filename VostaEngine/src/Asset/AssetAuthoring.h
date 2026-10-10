#pragma once

#include "Core/Core.h"
#include "Core/Json.h"

#include <string>

namespace ve {

	// JSON <-> on-disk asset bridge, used by the MCP asset tools so a model can
	// author the same files the editor writes. Only the text-based kinds are
	// covered (material, layered_material, material_layer, noise); textures and
	// meshes must still be imported.
	//
	// Every entry point returns a JSON string. Failure is {"error": "..."} with a
	// message meant to be read by a model that will retry, so nothing here throws
	// and a half-built graph (dangling wire, unknown node) is reported rather than
	// crashing.

	// Node-type catalogue for one asset kind, as JSON. `kind` is "material" or
	// "noise"; an empty string returns both under those keys. Each entry carries
	// the node's input/output pin names (in pin-index order, which is what a link
	// refers to) and the params it can carry.
	VE_API std::string assetNodeTypesJson(const std::string& kind);

	// Describe the asset at `path` as JSON, tagged with its "kind". `path` is
	// project-content-relative (e.g. "materials/rusty.veasset"), absolute, or in
	// the engine-root-relative form list_assets returns.
	VE_API std::string readAssetJson(const std::string& path);

	// Write the asset described by `data` (an object carrying "kind") to `path`.
	// overwrite=false reports an error when the file already exists.
	VE_API std::string writeAssetJson(const std::string& path, const JsonReader& data, bool overwrite);

	// Bake the noise graph at `source` into a square grayscale RGBA texture asset
	// at `out`. Both paths accept the same forms as read_asset. `size` is the
	// resolution in pixels (clamped to [8, 4096]); overwrite=false refuses an
	// existing `out`. Writes through TextureImporter, so the file is identical to
	// an imported texture.
	VE_API std::string bakeNoiseToTexture(const std::string& source, const std::string& out,
	                                      int size, bool overwrite);

}

#include "vepch.h"
#include "MCP/ToolRegistry.h"

#include "Asset/AssetAuthoring.h"

#include <string>

namespace ve {
	namespace {

		// ── tools ──────────────────────────────────────────────────────────
		// Thin wrappers over AssetAuthoring, which owns all the JSON <-> asset
		// knowledge. Each returns a JSON string; failure is {"error": "..."}.

		std::string listAssetNodeTypes(const JsonReader& args) {
			return assetNodeTypesJson(args.getString("kind", ""));
		}

		std::string readAsset(const JsonReader& args) {
			const std::string path = args.getString("path", "");
			if (path.empty())
				return toolError("missing 'path' (call list_assets for the paths in this project)");
			return readAssetJson(path);
		}

		std::string writeAssetCommon(const JsonReader& args, bool overwrite) {
			const std::string path = args.getString("path", "");
			if (path.empty())
				return toolError("missing 'path'");
			if (!args.has("data"))
				return toolError("missing 'data' object (the asset body, which must carry a 'kind')");
			return writeAssetJson(path, args.child("data"), overwrite);
		}

		std::string createAsset(const JsonReader& args) { return writeAssetCommon(args, false); }
		std::string overwriteAsset(const JsonReader& args) { return writeAssetCommon(args, true); }

		// ── registration ───────────────────────────────────────────────────

		ToolRegistrar reg_listAssetNodeTypes{
			"list_asset_node_types",
			"List the node types you can author inside a 'material' or 'noise' graph asset. Each entry gives the node's string name, its input and output pin names (comma-separated, in pin-index order - a link refers to those indices) and its editable params. Call this BEFORE create_asset/write_asset when building a graph. Pass 'kind' for one kind, or omit it for both.",
			R"SCHEMA({"type":"object","properties":{"kind":{"type":"string","description":"'material' or 'noise'; omit for both"}}})SCHEMA",
			listAssetNodeTypes
		};

		ToolRegistrar reg_readAsset{
			"read_asset",
			"Read a project asset and return it as JSON. Covers 'material', 'layered_material', 'material_layer' and 'noise' .veasset files, node graphs included. Read one, change the JSON, then hand it to write_asset. 'path' is relative to the project's content folder (e.g. 'materials/rusty.veasset'), absolute, or whatever list_assets returned; the .veasset extension is optional.",
			R"SCHEMA({"type":"object","properties":{"path":{"type":"string","description":"e.g. 'materials/rusty.veasset', or a path from list_assets"}},"required":["path"]})SCHEMA",
			readAsset
		};

		ToolRegistrar reg_createAsset{
			"create_asset",
			"Create a new asset file. Fails if the file already exists - use write_asset to overwrite. 'data' fields by kind: material {albedoColor:[r,g,b], metallic, roughness, ao, emissiveColor:[r,g,b], textures:{albedo,normal,metallic,roughness,ao,emissive}, graph}; noise {graph}; material_layer {name, albedoMap, normalMap, roughnessMap, heightMap, tiling, useSlope, slopeMin, slopeMax, useHeight, heightMin, heightMax, noiseStrength, noiseScale, blend}; layered_material {layers:[material_layerPaths], heightScale, macroStrength, macroScale}. A graph is {nodes:[{id,type,x,y,params}],links:[{fromNode,fromPin,toNode,toPin}]} - call list_asset_node_types for the node and pin names. Texture/map fields take asset paths; the .veasset extension is optional.",
			R"SCHEMA({"type":"object","properties":{"path":{"type":"string","description":"e.g. 'materials/rusty' or 'noise/rock'; the .veasset extension is added if missing"},"data":{"type":"object","description":"the asset body; must carry 'kind'","properties":{"kind":{"type":"string","enum":["material","noise","material_layer","layered_material"]}},"required":["kind"]}},"required":["path","data"]})SCHEMA",
			createAsset
		};

		ToolRegistrar reg_writeAsset{
			"write_asset",
			"Overwrite an existing asset, or create it if it is missing. The argument and 'data' shape are identical to create_asset; use this once you have read an asset and edited it. Call read_asset first so you keep the fields you are not changing.",
			R"SCHEMA({"type":"object","properties":{"path":{"type":"string","description":"the asset to write, e.g. 'materials/rusty.veasset'"},"data":{"type":"object","description":"the asset body; must carry 'kind'","properties":{"kind":{"type":"string","enum":["material","noise","material_layer","layered_material"]}},"required":["kind"]}},"required":["path","data"]})SCHEMA",
			overwriteAsset
		};

	} // namespace
} // namespace ve

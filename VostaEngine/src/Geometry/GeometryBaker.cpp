#include "vepch.h"
#include "Geometry/GeometryBaker.h"
#include "Geometry/GeometryEvaluator.h"
#include "Asset/StaticMeshImporter.h"
#include "Core/ResourceManager.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/ThumbnailRenderer.h"

namespace ve {

	bool bakeGeometryGraphToAsset(const GeometryGraph& graph, const std::string& outAssetPath, std::string* outError) {
		auto setError = [&](const std::string& msg) {
			if (outError) *outError = msg;
			VE_CORE_WARN_PRINT("GeometryBaker: %s", msg.c_str());
		};

		std::string evalError;
		Ref<StaticMeshResource> mesh = evaluateGeometryGraph(graph, &evalError);
		if (!mesh) {
			setError(evalError.empty() ? "geometry evaluation failed" : evalError);
			return false;
		}

		if (!StaticMeshImporter::saveToAsset(mesh, toAbsolute(outAssetPath))) {
			setError("failed to write " + outAssetPath);
			return false;
		}

		// store() dedups by path and would hand back the previous GPU mesh, so drop it
		// first — otherwise a re-bake keeps rendering the old geometry.
		ResourceManager::remove<StaticMesh>(outAssetPath);
		const AssetHandle handle = ResourceManager::store<StaticMesh>(outAssetPath);
		if (handle.isValid())
			ThumbnailRenderer::invalidateMesh(handle);
		ThumbnailRenderer::bakeAllLoaded();
		return true;
	}

}

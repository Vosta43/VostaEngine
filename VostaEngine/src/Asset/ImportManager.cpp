#include "vepch.h"
#include "ImportManager.h"
#include "Utils.h"
#include "StaticMeshImporter.h"
#include "TextureImporter.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Material.h"
#include "Renderer/Texture.h"
#include "Renderer/ThumbnailRenderer.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace ve {

	namespace {

		// Copy a file into destDir under its own name. Returns the destination
		// path, or empty on failure (including source not found).
		std::string copyInto(const std::filesystem::path& source, const std::filesystem::path& destDir) {
			std::error_code ec;
			if (!std::filesystem::exists(source, ec)) {
				VE_CORE_WARN_PRINT("Import: source not found, skipping: %s", source.string().c_str());
				return {};
			}

			std::filesystem::create_directories(destDir, ec);
			const std::filesystem::path dest = destDir / source.filename();

			// Already in place (importing a file that lives in the content tree).
			if (std::filesystem::exists(dest, ec) && std::filesystem::equivalent(source, dest, ec))
				return dest.string();

			std::filesystem::copy_file(source, dest,
				std::filesystem::copy_options::overwrite_existing, ec);
			if (ec) {
				VE_CORE_WARN_PRINT("Import: failed to copy %s -> %s",
					source.string().c_str(), dest.string().c_str());
				return {};
			}
			return dest.string();
		}

		// "mtllib" targets referenced by an OBJ, as bare filenames.
		std::vector<std::string> objMtlRefs(const std::filesystem::path& objPath) {
			std::vector<std::string> refs;
			std::ifstream in(objPath);
			std::string line;
			while (std::getline(in, line)) {
				std::istringstream iss(line);
				std::string token;
				iss >> token;
				if (token != "mtllib")
					continue;
				std::string name;
				std::getline(iss, name);
				const auto first = name.find_first_not_of(" \t\r");
				const auto last = name.find_last_not_of(" \t\r");
				if (first != std::string::npos)
					refs.push_back(name.substr(first, last - first + 1));
			}
			return refs;
		}

		// Texture filenames referenced by an MTL (map_* / bump / disp lines).
		std::vector<std::string> mtlTextureRefs(const std::filesystem::path& mtlPath) {
			std::vector<std::string> refs;
			std::ifstream in(mtlPath);
			std::string line;
			while (std::getline(in, line)) {
				std::istringstream iss(line);
				std::string token;
				iss >> token;
				const bool isMap = token.rfind("map_", 0) == 0 || token == "bump" || token == "disp";
				if (!isMap)
					continue;

				// Keep the last non-option token; map lines may carry -s/-o args.
				std::string arg, file;
				while (iss >> arg) {
					if (!arg.empty() && arg[0] != '-')
						file = arg;
				}
				if (!file.empty())
					refs.push_back(std::filesystem::path(file).filename().string());
			}
			return refs;
		}

		// Choose where to bake a source's .veasset. A free name is used as-is. A
		// name already held by the same asset kind is this source's earlier bake,
		// so it is overwritten (re-import). A name held by a *different* kind —
		// e.g. sofa.obj's mesh vs sofa.png — gets a "_1", "_2", ... suffix so
		// neither source clobbers the other.
		std::string bakedAssetPathFor(const std::filesystem::path& destDir,
			const std::string& stem, const char* token) {
			std::error_code ec;
			for (int i = 0; ; ++i) {
				const std::string name = (i == 0 ? stem : stem + "_" + std::to_string(i)) + ".veasset";
				const std::filesystem::path candidate = destDir / name;
				if (!std::filesystem::exists(candidate, ec))
					return candidate.string();
				if (utils::peekAssetToken(candidate.string()) == token)
					return candidate.string();
			}
		}

	}

	void ImportManager::import(const std::string& sourcePath, const std::string& destDir) {

		if (sourcePath.empty() || destDir.empty()) {
			VE_CORE_ERROR_PRINT("Import: missing source or destination");
			return;
		}

		const std::string ext = utils::getExtension(sourcePath);
		const std::filesystem::path dest(destDir);
		const std::filesystem::path src(sourcePath);

		if (ext == ".obj") {
			const std::string localObj = copyInto(src, dest);
			if (localObj.empty())
				return;

			// Pull in the material library and its textures so the imported
			// mesh does not depend on files outside the content tree.
			for (const auto& mtlRef : objMtlRefs(src)) {
				const std::filesystem::path mtlSrc = src.parent_path() / mtlRef;
				if (copyInto(mtlSrc, dest).empty())
					continue;
				for (const auto& texRef : mtlTextureRefs(mtlSrc))
					copyInto(mtlSrc.parent_path() / texRef, dest);
			}

			auto resource = StaticMeshImporter::importFromFile(localObj);
			if (!resource || resource->vertexBuffer.empty()) {
				VE_CORE_ERROR_PRINT("Import: failed to parse %s", localObj.c_str());
				return;
			}

			const std::filesystem::path localObjPath(localObj);
			const std::string assetPath = bakedAssetPathFor(dest, localObjPath.stem().string(), "staticmesh");
			if (!StaticMeshImporter::saveToAsset(resource, assetPath))
				return;

			ResourceManager::store<StaticMesh>(assetPath);
			// Bake the preview now so the browser shows the mesh's thumbnail
			// instead of waiting for a panel to touch it.
			ThumbnailRenderer::bakeAllLoaded();
			VE_CORE_SUCCESS_PRINT("Imported mesh asset: %s", assetPath.c_str());
		}
		else if (ext == ".fbx") {
			const std::string localFbx = copyInto(src, dest);
			if (localFbx.empty())
				return;

			// Pull in the externally referenced textures so the mesh does not
			// depend on files outside the content tree. Embedded textures travel
			// inside the FBX and need no copy.
			for (const auto& texRef : StaticMeshImporter::referencedTextures(src.string()))
				copyInto(src.parent_path() / texRef, dest);

			auto resource = StaticMeshImporter::importFromFile(localFbx);
			if (!resource || resource->vertexBuffer.empty()) {
				VE_CORE_ERROR_PRINT("Import: failed to parse %s", localFbx.c_str());
				return;
			}

			const std::filesystem::path localFbxPath(localFbx);
			const std::string assetPath = bakedAssetPathFor(dest, localFbxPath.stem().string(), "staticmesh");
			if (!StaticMeshImporter::saveToAsset(resource, assetPath))
				return;

			ResourceManager::store<StaticMesh>(assetPath);
			ThumbnailRenderer::bakeAllLoaded();
			VE_CORE_SUCCESS_PRINT("Imported mesh asset: %s", assetPath.c_str());
		}
		else if (ext == ".png" || ext == ".jpg" || ext == ".hdr") {
			const std::string local = copyInto(src, dest);
			if (local.empty())
				return;

			// Bake the decoded pixels into a self-contained .veasset so later
			// loads never touch the source image again — and so the browser's
			// "is this source imported?" lookup finds a texture asset for it.
			auto resource = TextureImporter::importFromFile(local);
			if (!resource) {
				VE_CORE_ERROR_PRINT("Import: failed to decode %s", local.c_str());
				return;
			}

			const std::filesystem::path localPath(local);
			const std::string assetPath = bakedAssetPathFor(dest, localPath.stem().string(), "texture");
			TextureImporter::serialize(*resource, assetPath);
			ResourceManager::store<Texture2D>(assetPath);
			VE_CORE_SUCCESS_PRINT("Imported texture asset: %s", assetPath.c_str());
		}
		else if (ext == ".veasset") {
			copyInto(src, dest);
		}
		else {
			VE_CORE_WARN_PRINT("Import: unsupported file type '%s'", ext.c_str());
		}
	}
}

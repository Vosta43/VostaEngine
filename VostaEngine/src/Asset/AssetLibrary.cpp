#include "vepch.h"
#include "Asset/AssetLibrary.h"

#include "Asset/Utils.h"
#include "Core/AssetConfig.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>

namespace ve {

	namespace {

		std::string toLower(std::string s) {
			std::transform(s.begin(), s.end(), s.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		// Extension → kind for raw sources, .veasset token → kind for baked files.
		// Both built once from assetTypes().
		const std::unordered_map<std::string, size_t>& extIndex() {
			static const std::unordered_map<std::string, size_t> index = [] {
				std::unordered_map<std::string, size_t> m;
				const auto& types = assetTypes();
				for (size_t i = 0; i < types.size(); ++i)
					for (const std::string& ext : types[i].extensions)
						m[toLower(ext)] = i;
				return m;
			}();
			return index;
		}

		const std::unordered_map<std::string, size_t>& tokenIndex() {
			static const std::unordered_map<std::string, size_t> index = [] {
				std::unordered_map<std::string, size_t> m;
				const auto& types = assetTypes();
				for (size_t i = 0; i < types.size(); ++i)
					if (!types[i].token.empty())
						m[types[i].token] = i;
				return m;
			}();
			return index;
		}

		// Fills `out` when `absPath` is a recognised asset, returns false otherwise.
		// Never throws.
		bool classify(const std::string& absPath, AssetEntry& out) {
			const std::string ext = toLower(utils::getExtension(absPath));
			const std::vector<AssetTypeInfo>& types = assetTypes();

			const AssetTypeInfo* type = nullptr;
			if (ext == ".veasset") {
				// A baked file's kind lives in its type token, not its extension.
				const std::string token = utils::peekAssetToken(absPath);
				if (token.empty())
					return false;
				auto it = tokenIndex().find(token);
				if (it == tokenIndex().end())
					return false;
				type = &types[it->second];
			}
			else {
				auto it = extIndex().find(ext);
				if (it == extIndex().end())
					return false;
				type = &types[it->second];
			}

			out.path = toRelative(absPath);
			out.name = std::filesystem::path(absPath).stem().string();
			out.typeKey = type->typeKey;
			out.category = type->category;
			return true;
		}

	} // namespace

	const std::vector<AssetTypeInfo>& assetTypes() {
		static const std::vector<AssetTypeInfo> types = {
			{ "staticmesh",       "Static Mesh",      "Mesh",     "staticmesh",       { ".obj", ".fbx" } },
			{ "texture",          "Texture",          "Texture",  "texture",          { ".png", ".jpg", ".jpeg", ".hdr" } },
			{ "material",         "Material",         "Material", "material",         {} },
			{ "layered_material", "Layered Material", "Material", "layered_material", {} },
			{ "material_layer",   "Material Layer",   "Material", "material_layer",   {} },
			{ "noise",            "Noise",            "Texture",  "noise",            {} },
			{ "terrain_data",     "Terrain Data",     "Terrain",  "terrain_data",     {} },
			{ "scene",            "Scene",            "Scene",    "",                 { ".veworld" } },
		};
		return types;
	}

	AssetLibrary& AssetLibrary::get() {
		static AssetLibrary instance;
		return instance;
	}

	void AssetLibrary::refresh() {
		m_entries.clear();

		const std::string rootStr = toProjectAbsolute("content");
		if (rootStr.empty())
			return;

		std::error_code ec;
		const std::filesystem::path root(rootStr);
		if (!std::filesystem::is_directory(root, ec))
			return;

		// Walk without following symlinks; a permission error skips rather than ends
		// the scan. increment(ec) keeps a broken entry from throwing out of here.
		std::filesystem::recursive_directory_iterator it(
			root, std::filesystem::directory_options::skip_permission_denied, ec);
		const std::filesystem::recursive_directory_iterator end;
		for (; it != end; it.increment(ec)) {
			if (ec)
				break;

			std::error_code entryEc;
			if (!it->is_regular_file(entryEc))
				continue;

			AssetEntry entry;
			if (classify(it->path().string(), entry))
				m_entries.push_back(std::move(entry));
		}

		std::sort(m_entries.begin(), m_entries.end(),
				  [](const AssetEntry& a, const AssetEntry& b) {
					  if (a.category != b.category) return a.category < b.category;
					  if (a.typeKey != b.typeKey)   return a.typeKey < b.typeKey;
					  return a.path < b.path;
				  });
	}

	const std::vector<AssetEntry>& AssetLibrary::list() const {
		return m_entries;
	}

	uint64_t AssetLibrary::revision() const {
		return m_revision;
	}

	void AssetLibrary::notifyChanged() {
		++m_revision;
	}

} // namespace ve

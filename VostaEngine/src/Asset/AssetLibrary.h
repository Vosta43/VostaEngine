#pragma once

#include "Core/Core.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ve {

	// One asset found on disk. `path` is asset-root-relative — the same string
	// ResourceManager stores and set_component accepts, so a listed path can be
	// handed straight back to the engine.
	struct AssetEntry {
		std::string path;
		std::string name;
		std::string typeKey;
		std::string category;
	};

	// A kind of asset the library recognises. `token` is the .veasset type tag
	// (empty when the kind has no baked form); `extensions` are the raw source
	// extensions that map to this kind directly (.obj, .png, ...).
	struct AssetTypeInfo {
		std::string typeKey;
		std::string displayName;
		std::string category;
		std::string token;
		std::vector<std::string> extensions;
	};

	// The fixed catalogue of asset kinds this build knows. Read-only.
	VE_API const std::vector<AssetTypeInfo>& assetTypes();

	// Enumerates the assets under the open project's content/ folder. Holds the
	// last scan only; refresh() rescans so a caller sees assets created since.
	class VE_API AssetLibrary {
	public:
		static AssetLibrary& get();

		void refresh();
		const std::vector<AssetEntry>& list() const;

		// Bumped by every writer that changes an asset on disk (save, AI write).
		// A view that shows the whole catalogue polls this and rescans when it
		// moves, instead of every writer knowing which views exist. A view with ONE
		// asset open does not use this — it re-reads that file's write time instead
		// (utils::fileWriteTicks), which also catches writers outside this process.
		uint64_t revision() const;

		// Record that some asset changed on disk. Bumps the listing revision so a
		// browser rescan; carries no per-asset detail by design.
		void notifyChanged();

	private:
		std::vector<AssetEntry> m_entries;
		uint64_t m_revision = 0;
	};

} // namespace ve

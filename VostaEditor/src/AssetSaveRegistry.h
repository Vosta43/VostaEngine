#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ve {

	// Editor-side registry of assets holding unsaved in-memory edits. Each entry is
	// an asset path plus the callback that writes it back to disk. Editors mark an
	// entry dirty as the user edits, and clean once it is written; the File
	// Browser's Save button runs saveAll() over everything that is still dirty.
	//
	// A saver must capture whatever it needs to write the file — the path and the
	// live resource Ref — NOT the editor window, which may close while its asset
	// is still dirty.
	class AssetSaveRegistry {
	public:
		static AssetSaveRegistry& get() {
			static AssetSaveRegistry instance;
			return instance;
		}

		// Register (or refresh) how to save `path` and count it as dirty.
		void markDirty(const std::string& path, std::function<void()> save) {
			if (path.empty()) return;
			m_savers[path] = std::move(save);
		}

		void markClean(const std::string& path) {
			m_savers.erase(path);
		}

		bool isDirty(const std::string& path) const {
			return m_savers.find(path) != m_savers.end();
		}

		std::size_t dirtyCount() const { return m_savers.size(); }

		// Write every dirty asset, dropping each entry it saved. Returns how many
		// were written.
		std::size_t saveAll() {
			std::vector<std::string> paths;
			paths.reserve(m_savers.size());
			for (const auto& kv : m_savers)
				paths.push_back(kv.first);

			std::size_t saved = 0;
			for (const std::string& path : paths) {
				auto it = m_savers.find(path);
				if (it == m_savers.end())
					continue;   // dropped by a saver that ran earlier this pass
				std::function<void()> save = it->second;
				if (save) save();
				m_savers.erase(it);
				++saved;
			}
			return saved;
		}

	private:
		std::unordered_map<std::string, std::function<void()>> m_savers;
	};

}

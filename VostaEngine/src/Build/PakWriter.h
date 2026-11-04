#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace ve {
namespace pak {

// Writes one PAK archive: file data chunked and zstd-compressed in place, then an
// index, then the footer a reader seeks to from the end of the file.
class Writer {
public:
	// Truncates `path`; fails when the file cannot be created.
	bool open(const std::string& path, std::string* error = nullptr);

	// Adds one source file under pakPath ('/'-separated, package-root-relative).
	// Paths are unique per archive; a duplicate is an error.
	bool addFile(const std::string& pakPath, const std::string& sourcePath,
	             int level = 3, std::string* error = nullptr);

	// Writes the index and footer. No further files can be added afterwards.
	bool finish(std::string* error = nullptr);

	uint64_t fileCount() const { return m_entries.size(); }

private:
	struct Chunk {
		uint32_t storedSize = 0;
		uint8_t  method = 0;
	};

	struct Entry {
		std::string path;
		uint64_t rawSize = 0;
		uint64_t dataOffset = 0;
		uint32_t chunkSize = 0;
		uint32_t chunkCount = 0;
		uint8_t  flags = 0;
		uint64_t rawHash = 0;
		std::vector<Chunk> chunks;
	};

	std::ofstream m_out;
	std::vector<Entry> m_entries;
	std::unordered_set<std::string> m_paths;
	bool m_finished = false;
};

} // namespace pak
} // namespace ve

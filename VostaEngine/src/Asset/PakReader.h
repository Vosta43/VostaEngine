#pragma once

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ve {
namespace pak {

// Read-only view of a PAK archive. The index is parsed once on open; file data
// is decompressed per 64 KiB chunk so random access only pays for the chunks it
// touches. Methods are const and internally serialised: one Reader can be shared
// across loading threads.
class Reader {
public:
	Reader() = default;
	~Reader();

	Reader(const Reader&) = delete;
	Reader& operator=(const Reader&) = delete;

	bool open(const std::string& path, std::string* error = nullptr);
	void close();
	bool isOpen() const { return m_open; }

	// Sorted, '/'-separated paths as stored in the archive.
	bool list(std::vector<std::string>& files) const;
	bool fileSize(const std::string& path, uint64_t& size) const;

	bool readFile(const std::string& path, std::vector<uint8_t>& out,
	              std::string* error = nullptr) const;
	bool readAt(const std::string& path, uint64_t offset, void* dst, size_t size,
	            std::string* error = nullptr) const;

	// Decompresses the entry and checks it against the xxhash64 recorded by the
	// writer. This is what the packager's self-check uses.
	bool verifyFile(const std::string& path, std::string* error = nullptr) const;

private:
	struct Chunk {
		uint32_t storedSize = 0;
		uint8_t  method = 0;
	};

	struct Entry {
		uint64_t rawSize = 0;
		uint64_t dataOffset = 0;
		uint32_t chunkSize = 0;
		uint32_t chunkCount = 0;
		uint8_t  flags = 0;
		uint64_t rawHash = 0;
		std::vector<Chunk> chunks;

		uint64_t chunkRawSize(uint32_t index) const {
			return index + 1 < chunkCount
				? chunkSize
				: rawSize - static_cast<uint64_t>(index) * chunkSize;
		}
	};

	const Entry* find(const std::string& path) const;
	bool loadChunk(const Entry& entry, uint32_t index, std::vector<uint8_t>& out,
	               std::string* error) const;
	bool readRange(const Entry& entry, uint64_t offset, void* dst, size_t size,
	               std::string* error) const;

	mutable std::ifstream m_file;
	mutable std::mutex m_mutex;
	std::unordered_map<std::string, Entry> m_entries;
	std::string m_path;
	bool m_open = false;
};

} // namespace pak
} // namespace ve

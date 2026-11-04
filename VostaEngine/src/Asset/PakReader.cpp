#include "vepch.h"
#include "Asset/PakReader.h"

#include "Asset/PakFormat.h"
#include "Core/Compression.h"

#include "common/xxhash.h"

#include <algorithm>
#include <cstring>

namespace ve {
namespace pak {

	namespace {

		void setError(std::string* error, const std::string& message) {
			if (error) *error = message;
		}

		// Bounds-checked little-endian cursor over the index bytes.
		class Cursor {
		public:
			Cursor(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

			bool u8(uint8_t& value) {
				if (m_pos + 1 > m_size) return false;
				value = m_data[m_pos++];
				return true;
			}

			bool u32(uint32_t& value) {
				if (m_pos + 4 > m_size) return false;
				value = 0;
				for (int i = 0; i < 4; ++i)
					value |= static_cast<uint32_t>(m_data[m_pos + i]) << (i * 8);
				m_pos += 4;
				return true;
			}

			bool u64(uint64_t& value) {
				if (m_pos + 8 > m_size) return false;
				value = 0;
				for (int i = 0; i < 8; ++i)
					value |= static_cast<uint64_t>(m_data[m_pos + i]) << (i * 8);
				m_pos += 8;
				return true;
			}

			bool bytes(const uint8_t*& data, size_t count) {
				if (m_pos + count > m_size) return false;
				data = m_data + m_pos;
				m_pos += count;
				return true;
			}

			bool string(std::string& value, size_t maxLength) {
				uint32_t length = 0;
				if (!u32(length) || length > maxLength || m_pos + length > m_size)
					return false;
				value.assign(reinterpret_cast<const char*>(m_data + m_pos), length);
				m_pos += length;
				return true;
			}

			bool atEnd() const { return m_pos == m_size; }
			size_t remaining() const { return m_size - m_pos; }

		private:
			const uint8_t* m_data;
			size_t m_size;
			size_t m_pos = 0;
		};

		// Same canonical form the writer stores: '/' separated, relative, and with
		// no component that could escape the archive root.
		bool validPath(const std::string& path) {
			if (path.empty() || path.size() > 4096 || path.front() == '/' ||
			    path.find('\\') != std::string::npos || path.find(':') != std::string::npos)
				return false;

			size_t start = 0;
			for (;;) {
				const size_t end = path.find('/', start);
				const std::string part = path.substr(
					start, end == std::string::npos ? std::string::npos : end - start);
				if (part.empty() || part == "." || part == "..") return false;
				if (end == std::string::npos) break;
				start = end + 1;
			}
			return true;
		}

	} // namespace

	Reader::~Reader() {
		close();
	}

	bool Reader::open(const std::string& path, std::string* error) {
		close();

		std::ifstream file(path, std::ios::binary);
		if (!file) {
			setError(error, "cannot open " + path);
			return false;
		}
		file.seekg(0, std::ios::end);
		const std::streamoff fileSize = file.tellg();
		if (fileSize < static_cast<std::streamoff>(kFooterSize)) {
			setError(error, "not a VEGPAK archive (too small): " + path);
			return false;
		}

		file.seekg(fileSize - static_cast<std::streamoff>(kFooterSize), std::ios::beg);
		uint8_t footerBytes[kFooterSize] = {};
		file.read(reinterpret_cast<char*>(footerBytes), kFooterSize);
		if (static_cast<size_t>(file.gcount()) != kFooterSize) {
			setError(error, "cannot read the archive footer: " + path);
			return false;
		}

		Cursor footer(footerBytes, kFooterSize);
		const uint8_t* magic = nullptr;
		uint32_t version = 0, flags = 0, fileCount = 0, reserved = 0;
		uint64_t indexOffset = 0, indexSize = 0;
		if (!footer.bytes(magic, sizeof(kMagic)) ||
		    std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
			setError(error, "not a VEGPAK archive: " + path);
			return false;
		}
		if (!footer.u32(version) || version != kFormatVersion) {
			setError(error, "unsupported VEGPAK version: " + path);
			return false;
		}
		if (!footer.u32(flags) || !footer.u64(indexOffset) || !footer.u64(indexSize) ||
		    !footer.u32(fileCount) || !footer.u32(reserved) || !footer.atEnd()) {
			setError(error, "corrupt archive footer: " + path);
			return false;
		}

		const uint64_t contentLimit = static_cast<uint64_t>(fileSize) - kFooterSize;
		if (indexSize < 4 || indexOffset > contentLimit || indexSize > contentLimit - indexOffset) {
			setError(error, "archive index is outside the file: " + path);
			return false;
		}

		std::vector<uint8_t> index(static_cast<size_t>(indexSize));
		file.seekg(static_cast<std::streamoff>(indexOffset), std::ios::beg);
		file.read(reinterpret_cast<char*>(index.data()), static_cast<std::streamsize>(index.size()));
		if (static_cast<size_t>(file.gcount()) != index.size()) {
			setError(error, "cannot read the archive index: " + path);
			return false;
		}

		Cursor cursor(index.data(), index.size());
		uint32_t count = 0;
		if (!cursor.u32(count) || count != fileCount || count > index.size() / 37) {
			setError(error, "corrupt archive index: " + path);
			return false;
		}

		std::unordered_map<std::string, Entry> entries;
		entries.reserve(count);
		for (uint32_t i = 0; i < count; ++i) {
			std::string entryPath;
			Entry entry;
			uint64_t rawSize = 0, dataOffset = 0, rawHash = 0;
			uint32_t chunkSize = 0, chunkCount = 0;
			uint8_t entryFlags = 0;
			if (!cursor.string(entryPath, 4096) || !validPath(entryPath) ||
			    !cursor.u64(rawSize) || !cursor.u64(dataOffset) ||
			    !cursor.u32(chunkSize) || !cursor.u32(chunkCount) ||
			    !cursor.u8(entryFlags) || !cursor.u64(rawHash)) {
				setError(error, "corrupt archive entry in " + path);
				return false;
			}
			if (chunkSize == 0 || chunkSize > 64u * 1024u * 1024u || dataOffset > indexOffset) {
				setError(error, "corrupt archive entry '" + entryPath + "'");
				return false;
			}

			const uint64_t expectedChunks = rawSize / chunkSize +
			                                (rawSize % chunkSize != 0 ? 1 : 0);
			if (expectedChunks != chunkCount) {
				setError(error, "chunk count mismatch for '" + entryPath + "'");
				return false;
			}
			// Every chunk record is 8 bytes; a larger count could not fit in the
			// index and would only pre-allocate a vector for a corrupt archive.
			if (chunkCount > cursor.remaining() / 8) {
				setError(error, "chunk table exceeds the index for '" + entryPath + "'");
				return false;
			}

			entry.rawSize    = rawSize;
			entry.dataOffset = dataOffset;
			entry.chunkSize  = chunkSize;
			entry.chunkCount = chunkCount;
			entry.flags      = entryFlags;
			entry.rawHash    = rawHash;
			entry.chunks.resize(chunkCount);

			uint64_t storedTotal = 0;
			for (uint32_t j = 0; j < chunkCount; ++j) {
				uint32_t storedSize = 0;
				uint8_t method = 0, reserved0 = 0, reserved1 = 0, reserved2 = 0;
				if (!cursor.u32(storedSize) || !cursor.u8(method) ||
				    !cursor.u8(reserved0) || !cursor.u8(reserved1) || !cursor.u8(reserved2)) {
					setError(error, "corrupt chunk table in '" + entryPath + "'");
					return false;
				}

				const uint64_t rawChunk = entry.chunkRawSize(j);
				uint64_t limit = 0;
				if (method == static_cast<uint8_t>(Method::Zstd))
					limit = compression::maxCompressedSize(static_cast<size_t>(rawChunk));
				else if (method == static_cast<uint8_t>(Method::Stored))
					limit = rawChunk;
				else {
					setError(error, "unknown compression method in '" + entryPath + "'");
					return false;
				}

				if (rawChunk == 0 || storedSize == 0 || storedSize > limit) {
					setError(error, "corrupt chunk in '" + entryPath + "'");
					return false;
				}
				if (storedTotal > UINT64_MAX - storedSize) {
					setError(error, "chunk table overflows in '" + entryPath + "'");
					return false;
				}
				storedTotal += storedSize;
				entry.chunks[j] = { storedSize, method };
			}

			if (storedTotal > indexOffset || dataOffset > indexOffset - storedTotal) {
				setError(error, "chunk data outside the content area in '" + entryPath + "'");
				return false;
			}
			if (!entries.emplace(entryPath, std::move(entry)).second) {
				setError(error, "duplicate archive path '" + entryPath + "'");
				return false;
			}
		}
		if (!cursor.atEnd()) {
			setError(error, "trailing bytes in the archive index: " + path);
			return false;
		}

		m_file      = std::move(file);
		m_entries   = std::move(entries);
		m_path      = path;
		m_open      = true;
		return true;
	}

	void Reader::close() {
		m_file.close();
		m_entries.clear();
		m_path.clear();
		m_open = false;
	}

	const Reader::Entry* Reader::find(const std::string& path) const {
		std::string key = path;
		std::replace(key.begin(), key.end(), '\\', '/');
		const auto it = m_entries.find(key);
		return it == m_entries.end() ? nullptr : &it->second;
	}

	bool Reader::list(std::vector<std::string>& files) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_open) return false;
		files.clear();
		files.reserve(m_entries.size());
		for (const auto& pair : m_entries)
			files.push_back(pair.first);
		std::sort(files.begin(), files.end());
		return true;
	}

	bool Reader::fileSize(const std::string& path, uint64_t& size) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_open) return false;
		const Entry* entry = find(path);
		if (!entry) return false;
		size = entry->rawSize;
		return true;
	}

	bool Reader::readFile(const std::string& path, std::vector<uint8_t>& out,
	                      std::string* error) const {
		out.clear();
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_open) {
			setError(error, "archive is not open");
			return false;
		}

		const Entry* entry = find(path);
		if (!entry) {
			setError(error, "not in archive: " + path);
			return false;
		}
		if (entry->rawSize > static_cast<uint64_t>(SIZE_MAX)) {
			setError(error, "file does not fit in memory: " + path);
			return false;
		}

		out.resize(static_cast<size_t>(entry->rawSize));
		if (out.empty()) return true;
		return readRange(*entry, 0, out.data(), out.size(), error);
	}

	bool Reader::readAt(const std::string& path, uint64_t offset, void* dst, size_t size,
	                    std::string* error) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_open) {
			setError(error, "archive is not open");
			return false;
		}

		const Entry* entry = find(path);
		if (!entry) {
			setError(error, "not in archive: " + path);
			return false;
		}
		if (!dst && size != 0) {
			setError(error, "null destination for " + path);
			return false;
		}
		if (offset > entry->rawSize || size > entry->rawSize - offset) {
			setError(error, "read range outside '" + path + "'");
			return false;
		}
		if (size == 0) return true;
		return readRange(*entry, offset, dst, size, error);
	}

	bool Reader::verifyFile(const std::string& path, std::string* error) const {
		std::vector<uint8_t> data;
		if (!readFile(path, data, error)) return false;

		uint64_t expectedHash = 0;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_open) {
				setError(error, "archive is not open");
				return false;
			}
			const Entry* entry = find(path);
			if (!entry) {
				setError(error, "not in archive: " + path);
				return false;
			}
			expectedHash = entry->rawHash;
		}

		uint8_t empty = 0;
		const void* ptr = data.empty() ? &empty : data.data();
		const uint64_t hash = static_cast<uint64_t>(XXH64(ptr, data.size(), 0));
		if (hash != expectedHash) {
			setError(error, "hash mismatch for " + path);
			return false;
		}
		return true;
	}

	bool Reader::loadChunk(const Entry& entry, uint32_t index, std::vector<uint8_t>& out,
	                       std::string* error) const {
		if (index >= entry.chunkCount) {
			setError(error, "chunk index outside the entry");
			return false;
		}

		uint64_t offset = entry.dataOffset;
		for (uint32_t i = 0; i < index; ++i)
			offset += entry.chunks[i].storedSize;

		const Chunk& chunk = entry.chunks[index];
		m_file.clear();
		m_file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
		if (!m_file) {
			setError(error, "cannot seek to a chunk");
			return false;
		}

		std::vector<uint8_t> stored(chunk.storedSize);
		m_file.read(reinterpret_cast<char*>(stored.data()),
		            static_cast<std::streamsize>(stored.size()));
		if (static_cast<size_t>(m_file.gcount()) != stored.size()) {
			setError(error, "truncated chunk data");
			return false;
		}

		const uint64_t rawSize = entry.chunkRawSize(index);
		if (chunk.method == static_cast<uint8_t>(Method::Stored)) {
			if (stored.size() != rawSize) {
				setError(error, "stored chunk size mismatch");
				return false;
			}
			out = std::move(stored);
			return true;
		}
		return compression::decompress(stored.data(), stored.size(), rawSize, out, error);
	}

	bool Reader::readRange(const Entry& entry, uint64_t offset, void* dst, size_t size,
	                       std::string* error) const {
		uint8_t* bytes = static_cast<uint8_t*>(dst);
		std::vector<uint8_t> chunk;
		size_t done = 0;
		while (done < size) {
			const uint64_t absolute = offset + done;
			const uint64_t chunkIndex = absolute / entry.chunkSize;
			if (chunkIndex >= entry.chunkCount) {
				setError(error, "read ran past the last chunk");
				return false;
			}

			const uint32_t index = static_cast<uint32_t>(chunkIndex);
			const size_t within = static_cast<size_t>(absolute % entry.chunkSize);
			if (!loadChunk(entry, index, chunk, error)) return false;
			if (within >= chunk.size()) {
				setError(error, "chunk shorter than its declared size");
				return false;
			}

			const size_t n = std::min(chunk.size() - within, size - done);
			std::memcpy(bytes + done, chunk.data() + within, n);
			done += n;
		}
		return true;
	}

} // namespace pak
} // namespace ve

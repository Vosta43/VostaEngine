#include "vepch.h"
#include "Build/PakWriter.h"

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

		// Stored paths are lookup keys in the reader, so they are kept in one
		// canonical form: '/' separators and no "." / ".." components that could
		// escape the package root.
		bool normalizePath(std::string& path, std::string* error) {
			std::replace(path.begin(), path.end(), '\\', '/');
			while (path.rfind("./", 0) == 0) path.erase(0, 2);
			if (path.empty() || path.front() == '/' || path.size() > 4096 ||
			    path.find(':') != std::string::npos) {
				setError(error, "unusable archive path: " + path);
				return false;
			}

			size_t start = 0;
			for (;;) {
				const size_t end = path.find('/', start);
				const std::string part = path.substr(
					start, end == std::string::npos ? std::string::npos : end - start);
				if (part.empty() || part == "." || part == "..") {
					setError(error, "unusable archive path: " + path);
					return false;
				}
				if (end == std::string::npos) break;
				start = end + 1;
			}
			return true;
		}

	} // namespace

	bool Writer::open(const std::string& path, std::string* error) {
		if (m_out.is_open()) {
			setError(error, "archive is already open");
			return false;
		}

		m_out.open(path, std::ios::binary | std::ios::trunc);
		if (!m_out) {
			setError(error, "cannot create " + path);
			return false;
		}
		return true;
	}

	bool Writer::addFile(const std::string& pakPath, const std::string& sourcePath,
	                     int level, std::string* error) {
		if (!m_out.is_open() || m_finished) {
			setError(error, "archive is not writable");
			return false;
		}

		std::string path = pakPath;
		if (!normalizePath(path, error)) return false;
		if (!m_paths.insert(path).second) {
			setError(error, "duplicate archive path: " + path);
			return false;
		}

		std::ifstream in(sourcePath, std::ios::binary);
		if (!in) {
			setError(error, "cannot open " + sourcePath);
			return false;
		}
		in.seekg(0, std::ios::end);
		const std::streamoff end = in.tellg();
		if (end < 0) {
			setError(error, "cannot size " + sourcePath);
			return false;
		}
		in.seekg(0, std::ios::beg);

		Entry entry;
		entry.path      = path;
		entry.rawSize   = static_cast<uint64_t>(end);
		entry.chunkSize = kChunkSize;
		const uint64_t chunkCount = entry.rawSize / kChunkSize +
		                            (entry.rawSize % kChunkSize != 0 ? 1 : 0);
		if (chunkCount > 0xFFFFFFFFull) {
			setError(error, "file too large for the chunk index: " + path);
			return false;
		}
		entry.chunkCount = static_cast<uint32_t>(chunkCount);
		const std::streampos start = m_out.tellp();
		if (start < 0) {
			setError(error, "cannot locate the data for " + path);
			return false;
		}
		entry.dataOffset = static_cast<uint64_t>(static_cast<std::streamoff>(start));

		XXH64_state_t* hash = XXH64_createState();
		if (!hash) {
			setError(error, "out of memory while hashing " + path);
			return false;
		}
		XXH64_reset(hash, 0);

		std::vector<uint8_t> raw(kChunkSize);
		std::vector<uint8_t> compressed;
		uint64_t remaining = entry.rawSize;
		while (remaining > 0) {
			const size_t n = static_cast<size_t>(std::min<uint64_t>(remaining, kChunkSize));
			in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(n));
			if (static_cast<size_t>(in.gcount()) != n) {
				XXH64_freeState(hash);
				setError(error, "read failed: " + sourcePath);
				return false;
			}
			XXH64_update(hash, raw.data(), n);

			std::string compressError;
			if (!compression::compress(raw.data(), n, compressed, level, &compressError)) {
				XXH64_freeState(hash);
				setError(error, "compressing " + path + ": " + compressError);
				return false;
			}

			Chunk chunk;
			if (!compressed.empty() && compressed.size() < n) {
				chunk.method     = static_cast<uint8_t>(Method::Zstd);
				chunk.storedSize = static_cast<uint32_t>(compressed.size());
				m_out.write(reinterpret_cast<const char*>(compressed.data()),
				            static_cast<std::streamsize>(compressed.size()));
			} else {
				chunk.method     = static_cast<uint8_t>(Method::Stored);
				chunk.storedSize = static_cast<uint32_t>(n);
				m_out.write(reinterpret_cast<const char*>(raw.data()),
				            static_cast<std::streamsize>(n));
			}
			if (!m_out) {
				XXH64_freeState(hash);
				setError(error, "write failed for " + path);
				return false;
			}

			entry.chunks.push_back(chunk);
			remaining -= n;
		}

		entry.rawHash = XXH64_digest(hash);
		XXH64_freeState(hash);
		m_entries.push_back(std::move(entry));
		return true;
	}

	bool Writer::finish(std::string* error) {
		if (!m_out.is_open() || m_finished) {
			setError(error, "archive is not writable");
			return false;
		}
		if (m_entries.size() > 0xFFFFFFFFull) {
			setError(error, "too many files for one archive");
			return false;
		}

		const std::streampos indexOffset = m_out.tellp();
		if (indexOffset < 0) {
			setError(error, "cannot locate the index");
			return false;
		}

		writeU32(m_out, static_cast<uint32_t>(m_entries.size()));
		for (const Entry& entry : m_entries) {
			writeString(m_out, entry.path);
			writeU64(m_out, entry.rawSize);
			writeU64(m_out, entry.dataOffset);
			writeU32(m_out, entry.chunkSize);
			writeU32(m_out, entry.chunkCount);
			writeU8(m_out, entry.flags);
			writeU64(m_out, entry.rawHash);
			for (const Chunk& chunk : entry.chunks) {
				writeU32(m_out, chunk.storedSize);
				writeU8(m_out, chunk.method);
				writeU8(m_out, 0);
				writeU8(m_out, 0);
				writeU8(m_out, 0);
			}
		}

		const std::streampos indexEnd = m_out.tellp();
		if (indexEnd < indexOffset) {
			setError(error, "cannot size the index");
			return false;
		}
		const uint64_t indexSize = static_cast<uint64_t>(indexEnd - indexOffset);

		writeMagic(m_out);
		writeU32(m_out, kFormatVersion);
		writeU32(m_out, 0);  // flags: reserved for a compressed/encrypted index
		writeU64(m_out, static_cast<uint64_t>(static_cast<std::streamoff>(indexOffset)));
		writeU64(m_out, indexSize);
		writeU32(m_out, static_cast<uint32_t>(m_entries.size()));
		writeU32(m_out, 0);

		m_out.flush();
		if (!m_out) {
			setError(error, "write failed while closing the archive");
			return false;
		}
		m_out.close();
		m_finished = true;
		return true;
	}

} // namespace pak
} // namespace ve

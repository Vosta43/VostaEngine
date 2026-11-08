#include "vepch.h"
#include "Build/PakWriter.h"

#include "Asset/PakFormat.h"
#include "Asset/PakKey.h"
#include "Core/Compression.h"

#include "common/xxhash.h"
#include "monocypher.h"

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

	Writer::~Writer() {
		wipeKey();
	}

	void Writer::wipeKey() {
		crypto_wipe(m_key, sizeof(m_key));
		m_encrypted = false;
	}

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

	bool Writer::openEncrypted(const std::string& path, uint32_t keyId,
	                           std::string* error) {
		if (m_out.is_open()) {
			setError(error, "archive is already open");
			return false;
		}
		if (!keyById(keyId, m_key)) {
			setError(error, "content key #" + std::to_string(keyId) + " is not available");
			return false;
		}
		if (!open(path, error)) {
			wipeKey();
			return false;
		}
		m_keyId     = keyId;
		m_encrypted = true;
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
		entry.flags     = m_encrypted ? kEntryEncrypted : 0;
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
		std::vector<uint8_t> sealed;
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
			const uint8_t* payload = raw.data();
			size_t payloadSize = n;
			chunk.method = static_cast<uint8_t>(Method::Stored);
			if (!compressed.empty() && compressed.size() < n) {
				payload     = compressed.data();
				payloadSize = compressed.size();
				chunk.method = static_cast<uint8_t>(Method::Zstd);
			}

			if (m_encrypted) {
				const uint32_t chunkIndex = static_cast<uint32_t>(entry.chunks.size());
				sealed.resize(kSealOverhead + payloadSize);
				if (!randomBytes(sealed.data(), kNonceSize)) {
					XXH64_freeState(hash);
					setError(error, "the system random number generator failed");
					return false;
				}
				const std::vector<uint8_t> aad = chunkAad(path, chunkIndex, n);
				uint8_t* cipher = sealed.data() + kNonceSize;
				uint8_t* mac    = cipher + payloadSize;
				crypto_aead_lock(cipher, mac, m_key, sealed.data(),
				                 aad.data(), aad.size(), payload, payloadSize);
				payload     = sealed.data();
				payloadSize = sealed.size();
			}

			chunk.storedSize = static_cast<uint32_t>(payloadSize);
			m_out.write(reinterpret_cast<const char*>(payload),
			            static_cast<std::streamsize>(payloadSize));
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

		std::vector<uint8_t> index;
		appendU32(index, static_cast<uint32_t>(m_entries.size()));
		for (const Entry& entry : m_entries) {
			appendString(index, entry.path);
			appendU64(index, entry.rawSize);
			appendU64(index, entry.dataOffset);
			appendU32(index, entry.chunkSize);
			appendU32(index, entry.chunkCount);
			appendU8(index, entry.flags);
			appendU64(index, entry.rawHash);
			for (const Chunk& chunk : entry.chunks) {
				appendU32(index, chunk.storedSize);
				appendU8(index, chunk.method);
				appendU8(index, 0);
				appendU8(index, 0);
				appendU8(index, 0);
			}
		}

		const uint32_t fileCount = static_cast<uint32_t>(m_entries.size());
		std::vector<uint8_t> storedIndex;
		uint32_t footerFlags = 0;
		if (m_encrypted) {
			footerFlags = kFooterIndexEncrypted | kFooterPayloadEncrypted;
			storedIndex.resize(kSealOverhead + index.size());
			if (!randomBytes(storedIndex.data(), kNonceSize)) {
				setError(error, "the system random number generator failed");
				return false;
			}
			const std::vector<uint8_t> aad =
				indexAad(static_cast<uint64_t>(static_cast<std::streamoff>(indexOffset)),
				         fileCount);
			uint8_t* cipher = storedIndex.data() + kNonceSize;
			uint8_t* mac    = cipher + index.size();
			crypto_aead_lock(cipher, mac, m_key, storedIndex.data(),
			                 aad.data(), aad.size(), index.data(), index.size());
		} else {
			storedIndex = std::move(index);
		}

		m_out.write(reinterpret_cast<const char*>(storedIndex.data()),
		            static_cast<std::streamsize>(storedIndex.size()));
		if (!m_out) {
			setError(error, "write failed while writing the index");
			return false;
		}

		std::vector<uint8_t> footer;
		footer.reserve(kFooterSize);
		footer.insert(footer.end(), kMagic, kMagic + sizeof(kMagic));
		appendU32(footer, kFormatVersion);
		appendU32(footer, footerFlags);
		appendU32(footer, m_keyId);
		appendU64(footer, static_cast<uint64_t>(static_cast<std::streamoff>(indexOffset)));
		appendU64(footer, storedIndex.size());
		appendU32(footer, fileCount);
		appendU32(footer, 0);

		m_out.write(reinterpret_cast<const char*>(footer.data()),
		            static_cast<std::streamsize>(footer.size()));

		m_out.flush();
		if (!m_out) {
			setError(error, "write failed while closing the archive");
			return false;
		}
		m_out.close();
		m_finished = true;
		wipeKey();
		return true;
	}

} // namespace pak
} // namespace ve

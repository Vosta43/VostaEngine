#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ve {
namespace pak {

	// The magic names the archive family; kFormatVersion names the layout. A
	// version bump may change the footer, so a reader must not guess.
	inline constexpr char kMagic[8] = { 'V', 'E', 'G', 'P', 'A', 'K', '0', '0' };
	inline constexpr uint32_t kFormatVersion = 2;
	inline constexpr uint32_t kChunkSize = 64u * 1024u;

	// magic + version + flags + keyId + indexOffset + indexSize + fileCount + reserved
	inline constexpr size_t kFooterSize = 8 + 4 + 4 + 4 + 8 + 8 + 4 + 4;

	// XChaCha20-Poly1305 (Monocypher). Every sealed message carries its own
	// random nonce, so no counter has to survive between writer instances.
	inline constexpr size_t kKeySize   = 32;
	inline constexpr size_t kNonceSize = 24;
	inline constexpr size_t kMacSize   = 16;
	inline constexpr size_t kSealOverhead = kNonceSize + kMacSize;

	enum FooterFlags : uint32_t {
		kFooterIndexEncrypted   = 1u << 0,
		kFooterPayloadEncrypted = 1u << 1,
	};

	enum EntryFlags : uint8_t {
		kEntryEncrypted = 1u << 0,
	};

	enum class Method : uint8_t {
		Stored = 0,
		Zstd   = 1,
	};

	// Every integer in the archive is little-endian regardless of the host.
	inline void appendU8(std::vector<uint8_t>& out, uint8_t value) {
		out.push_back(value);
	}

	inline void appendU32(std::vector<uint8_t>& out, uint32_t value) {
		for (int i = 0; i < 4; ++i)
			out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
	}

	inline void appendU64(std::vector<uint8_t>& out, uint64_t value) {
		for (int i = 0; i < 8; ++i)
			out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
	}

	inline void appendString(std::vector<uint8_t>& out, const std::string& value) {
		appendU32(out, static_cast<uint32_t>(value.size()));
		out.insert(out.end(), value.begin(), value.end());
	}

	// Associated data binds a sealed chunk to its entry and position: without the
	// path a chunk moved into another file would still authenticate.
	inline std::vector<uint8_t> chunkAad(const std::string& path, uint32_t chunkIndex,
	                                     uint64_t rawChunkSize) {
		std::vector<uint8_t> aad;
		aad.reserve(path.size() + 13);
		aad.push_back(1);
		aad.insert(aad.end(), path.begin(), path.end());
		appendU32(aad, chunkIndex);
		appendU64(aad, rawChunkSize);
		return aad;
	}

	inline std::vector<uint8_t> indexAad(uint64_t indexOffset, uint32_t fileCount) {
		std::vector<uint8_t> aad;
		aad.reserve(21);
		aad.push_back(2);
		appendU64(aad, indexOffset);
		appendU32(aad, fileCount);
		return aad;
	}

} // namespace pak
} // namespace ve

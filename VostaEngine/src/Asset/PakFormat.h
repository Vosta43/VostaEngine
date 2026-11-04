#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

namespace ve {
namespace pak {

	// "VEGPAK00" carries its generation in the last two bytes: an incompatible
	// layout gets a new magic rather than a reader guessing at the old one.
	inline constexpr char kMagic[8] = { 'V', 'E', 'G', 'P', 'A', 'K', '0', '0' };
	inline constexpr uint32_t kFormatVersion = 1;
	inline constexpr uint32_t kChunkSize = 64u * 1024u;

	// magic + version + flags + indexOffset + indexSize + fileCount + reserved
	inline constexpr size_t kFooterSize = 8 + 4 + 4 + 8 + 8 + 4 + 4;

	enum class Method : uint8_t {
		Stored = 0,
		Zstd   = 1,
	};

	// Every integer in the archive is little-endian regardless of the host.
	inline void writeU8(std::ostream& out, uint8_t value) {
		const char byte = static_cast<char>(value);
		out.write(&byte, 1);
	}

	inline void writeU32(std::ostream& out, uint32_t value) {
		char bytes[4];
		for (int i = 0; i < 4; ++i)
			bytes[i] = static_cast<char>((value >> (i * 8)) & 0xFF);
		out.write(bytes, 4);
	}

	inline void writeU64(std::ostream& out, uint64_t value) {
		char bytes[8];
		for (int i = 0; i < 8; ++i)
			bytes[i] = static_cast<char>((value >> (i * 8)) & 0xFF);
		out.write(bytes, 8);
	}

	inline void writeString(std::ostream& out, const std::string& value) {
		writeU32(out, static_cast<uint32_t>(value.size()));
		out.write(value.data(), static_cast<std::streamsize>(value.size()));
	}

	inline void writeMagic(std::ostream& out) {
		out.write(kMagic, sizeof(kMagic));
	}

} // namespace pak
} // namespace ve

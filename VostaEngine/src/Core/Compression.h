#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ve {
namespace compression {

	// Pass this as expectedRawSize when the writer did not record one: the raw
	// size is then read from the zstd frame header.
	inline constexpr uint64_t kUnknownRawSize = UINT64_MAX;

	// Worst-case compressed size for rawSize bytes; use it to reserve a buffer
	// when compressing into caller-owned memory.
	size_t maxCompressedSize(size_t rawSize);

	// Replaces `out` with a zstd frame. level follows zstd's 1..22 scale (3 is
	// zstd's default). On failure `out` is cleared and `error`, when given,
	// receives the zstd error name.
	bool compress(const void* data, size_t size, std::vector<uint8_t>& out,
	              int level = 3, std::string* error = nullptr);

	// Replaces `out` with the raw bytes. expectedRawSize is the size recorded by
	// the writer: a short decode is treated as corruption instead of being
	// returned with an uninitialised tail. On failure `out` is cleared.
	bool decompress(const void* data, size_t size, uint64_t expectedRawSize,
	                std::vector<uint8_t>& out, std::string* error = nullptr);

} // namespace compression
} // namespace ve

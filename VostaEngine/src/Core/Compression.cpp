#include "vepch.h"
#include "Core/Compression.h"

#include "zstd.h"

namespace ve {
namespace compression {

	namespace {

		void setError(std::string* error, const std::string& message) {
			if (error)
				*error = message;
		}

		void setError(std::string* error, const char* what, size_t code) {
			if (error)
				*error = std::string(what) + ZSTD_getErrorName(code);
		}

	} // namespace

	size_t maxCompressedSize(size_t rawSize) {
		return ZSTD_compressBound(rawSize);
	}

	bool compress(const void* data, size_t size, std::vector<uint8_t>& out,
	              int level, std::string* error) {
		if (!data && size != 0) {
			out.clear();
			setError(error, "compression input is null");
			return false;
		}

		uint8_t emptyInput = 0;
		const void* src = size != 0 ? data : &emptyInput;

		const size_t bound = ZSTD_compressBound(size);
		out.resize(bound);
		const size_t written = ZSTD_compress(out.data(), bound, src, size, level);
		if (ZSTD_isError(written)) {
			out.clear();
			setError(error, "compression failed: ", written);
			return false;
		}

		out.resize(written);
		return true;
	}

	bool decompress(const void* data, size_t size, uint64_t expectedRawSize,
	                std::vector<uint8_t>& out, std::string* error) {
		if (!data && size != 0) {
			out.clear();
			setError(error, "decompression input is null");
			return false;
		}

		uint64_t rawSize = expectedRawSize;
		if (rawSize == kUnknownRawSize) {
			rawSize = ZSTD_getFrameContentSize(data, size);
			if (rawSize == ZSTD_CONTENTSIZE_ERROR) {
				out.clear();
				setError(error, "input is not a zstd frame");
				return false;
			}
			if (rawSize == ZSTD_CONTENTSIZE_UNKNOWN) {
				out.clear();
				setError(error, "the frame does not record its raw size");
				return false;
			}
		}

		const size_t rawBytes = static_cast<size_t>(rawSize);
		if (static_cast<uint64_t>(rawBytes) != rawSize) {
			out.clear();
			setError(error, "raw size does not fit in memory");
			return false;
		}

		out.resize(rawBytes);
		// zstd wants a non-null destination even for an empty block; the dummy
		// keeps truncated-frame detection working when rawBytes is zero.
		uint8_t emptyDst = 0;
		uint8_t* dst = out.empty() ? &emptyDst : out.data();

		const size_t written = ZSTD_decompress(dst, out.size(), data, size);
		if (ZSTD_isError(written)) {
			out.clear();
			setError(error, "decompression failed: ", written);
			return false;
		}
		if (written != out.size()) {
			out.clear();
			setError(error, "decompressed size does not match the expected size");
			return false;
		}

		return true;
	}

} // namespace compression
} // namespace ve

#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace ve {

	// FNV-1a 64. The shared key tool for the derived-data cache and for
	// content-hash invalidation: a caller composes a key from an algorithm
	// version plus every input's content hash, so a changed input yields a new
	// key and the old derived data is simply never asked for again.
	inline constexpr uint64_t kFnvOffset = 0xcbf29ce484222325ULL;
	inline constexpr uint64_t kFnvPrime = 0x100000001b3ULL;

	inline uint64_t hashBytes(const void* data, size_t size, uint64_t seed = kFnvOffset) {
		const uint8_t* p = static_cast<const uint8_t*>(data);
		uint64_t h = seed;
		for (size_t i = 0; i < size; ++i) {
			h ^= static_cast<uint64_t>(p[i]);
			h *= kFnvPrime;
		}
		return h;
	}

	inline uint64_t hashString(std::string_view s, uint64_t seed = kFnvOffset) {
		return hashBytes(s.data(), s.size(), seed);
	}

	// mx3-style mix (same shape as Jolt's HashCombine), used to fold one
	// already-hashed value into a running key.
	inline uint64_t hashCombine(uint64_t seed, uint64_t value) {
		constexpr uint64_t c = 0xbea225f9eb34556dULL;
		uint64_t x = value * c;
		x ^= x >> 39;
		uint64_t h = seed + x * c;
		h *= c;
		h ^= h >> 32; h *= c;
		h ^= h >> 29; h *= c;
		h ^= h >> 32; h *= c;
		h ^= h >> 29;
		return h;
	}

	// Chain a trivially-copyable value into a running key by its bytes.
	// float/double normalize -0.0 to 0.0 first, so the two bit patterns don't
	// hash apart (they compare equal, so they must key equal).
	template <class T>
	inline uint64_t hashValue(uint64_t seed, const T& value) {
		if constexpr (std::is_same_v<T, float>) {
			const float v = (value == 0.0f) ? 0.0f : value;
			return hashBytes(&v, sizeof(v), seed);
		}
		else if constexpr (std::is_same_v<T, double>) {
			const double v = (value == 0.0) ? 0.0 : value;
			return hashBytes(&v, sizeof(v), seed);
		}
		else {
			static_assert(std::is_trivially_copyable_v<T>,
				"hashValue needs a trivially copyable type");
			return hashBytes(&value, sizeof(T), seed);
		}
	}

	// Hash a file's contents. Missing or unreadable files return the seed, so a
	// caller that failed to open the asset still gets a stable (if empty) hash.
	inline uint64_t hashFileBytes(const std::string& path, uint64_t seed = kFnvOffset) {
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return seed;

		uint64_t h = seed;
		char buf[64 * 1024];
		while (in) {
			in.read(buf, sizeof(buf));
			const std::streamsize n = in.gcount();
			if (n > 0)
				h = hashBytes(buf, static_cast<size_t>(n), h);
		}
		return h;
	}

}

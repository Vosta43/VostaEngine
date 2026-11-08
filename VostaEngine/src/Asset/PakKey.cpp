#include "vepch.h"
#include "Asset/PakKey.h"

#include <array>
#include <cstring>
#include <mutex>
#include <unordered_map>

#if defined(VE_PLATFORM_WINDOWS)
	#include <bcrypt.h>
	#pragma comment(lib, "bcrypt.lib")
#else
	#include <cstdio>
#endif

namespace ve {
namespace pak {

	namespace {

		// Stored masked so the key does not appear as a readable byte string in
		// the shipped DLL. This raises the bar for casual extraction, nothing more.
		constexpr std::array<uint8_t, kKeySize> kMaskedKey = {
			0xF2, 0xDE, 0xBB, 0xE6, 0xE9, 0xF9, 0x8E, 0xDB,
			0xA2, 0x62, 0x25, 0x2E, 0xAB, 0x64, 0x90, 0x04,
			0x3D, 0x41, 0xAD, 0x2A, 0x7F, 0xEF, 0xED, 0x80,
			0x0F, 0x72, 0x54, 0x0B, 0x7A, 0x1D, 0x37, 0x0D,
		};

		constexpr std::array<uint8_t, kKeySize> kKeyMask = {
			0x59, 0xDC, 0x5F, 0xE2, 0x65, 0xE8, 0x6B, 0xEE,
			0x71, 0xF4, 0x77, 0xFA, 0x7D, 0x00, 0x83, 0x06,
			0x89, 0x0C, 0x8F, 0x12, 0x95, 0x18, 0x9B, 0x1E,
			0xA1, 0x24, 0xA7, 0x2A, 0xAD, 0x30, 0xB3, 0x36,
		};

		std::mutex g_mutex;
		std::unordered_map<uint32_t, std::array<uint8_t, kKeySize>> g_keys;

		void decodeBuiltin(uint8_t out[kKeySize]) {
			for (size_t i = 0; i < kKeySize; ++i)
				out[i] = static_cast<uint8_t>(kMaskedKey[i] ^ kKeyMask[i]);
		}

	} // namespace

	bool keyById(uint32_t id, uint8_t out[kKeySize]) {
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			const auto it = g_keys.find(id);
			if (it != g_keys.end()) {
				std::memcpy(out, it->second.data(), kKeySize);
				return true;
			}
		}
		if (id != kBuiltinKeyId) return false;
		decodeBuiltin(out);
		return true;
	}

	bool registerKey(uint32_t id, const uint8_t key[kKeySize]) {
		if (!key) return false;
		std::array<uint8_t, kKeySize> copy = {};
		std::memcpy(copy.data(), key, kKeySize);
		std::lock_guard<std::mutex> lock(g_mutex);
		g_keys[id] = copy;
		return true;
	}

	bool randomBytes(void* out, size_t size) {
		if (!out && size != 0) return false;
		if (size == 0) return true;

#if defined(VE_PLATFORM_WINDOWS)
		return BCryptGenRandom(nullptr, static_cast<PUCHAR>(out), static_cast<ULONG>(size),
		                       BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
		std::FILE* file = std::fopen("/dev/urandom", "rb");
		if (!file) return false;
		const size_t read = std::fread(out, 1, size, file);
		std::fclose(file);
		return read == size;
#endif
	}

} // namespace pak
} // namespace ve

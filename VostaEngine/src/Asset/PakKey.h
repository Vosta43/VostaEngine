#pragma once

#include "Core.h"
#include "Asset/PakFormat.h"

#include <cstddef>
#include <cstdint>

namespace ve {
namespace pak {

	// Content keys are identified by id so one runtime can open archives built
	// for several projects. Id 0 is the key compiled into this engine; anything
	// else must be registered before the archive is opened.
	inline constexpr uint32_t kBuiltinKeyId = 0;

	VE_API bool keyById(uint32_t id, uint8_t out[kKeySize]);
	VE_API bool registerKey(uint32_t id, const uint8_t key[kKeySize]);

	// Cryptographic random bytes for nonces. Returns false when the OS generator
	// fails; callers must not fall back to a weaker source.
	VE_API bool randomBytes(void* out, size_t size);

} // namespace pak
} // namespace ve

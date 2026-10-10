#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ve::vellum {

	// Decodes one codepoint at `i`, advances `i` past it, and returns it. A
	// malformed or truncated sequence skips a single byte and yields 0 — the
	// caller treats 0 as "nothing to draw". This is a UI text path, not a
	// validating parser: it does not reject overlong forms or surrogates.
	inline uint32_t utf8Next(const std::string& s, std::size_t& i) {
		if (i >= s.size())
			return 0;

		const unsigned char c0 = static_cast<unsigned char>(s[i]);
		if (c0 < 0x80) { ++i; return c0; }

		auto continuation = [&](std::size_t k) {
			return i + k < s.size() && (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80;
		};
		auto payload = [&](std::size_t k) {
			return static_cast<uint32_t>(static_cast<unsigned char>(s[i + k]) & 0x3F);
		};

		uint32_t cp = 0;
		if ((c0 & 0xE0) == 0xC0 && continuation(1)) {
			cp = ((c0 & 0x1Fu) << 6) | payload(1);
			i += 2;
		}
		else if ((c0 & 0xF0) == 0xE0 && continuation(1) && continuation(2)) {
			cp = ((c0 & 0x0Fu) << 12) | (payload(1) << 6) | payload(2);
			i += 3;
		}
		else if ((c0 & 0xF8) == 0xF0 && continuation(1) && continuation(2) && continuation(3)) {
			cp = ((c0 & 0x07u) << 18) | (payload(1) << 12) | (payload(2) << 6) | payload(3);
			i += 4;
		}
		else {
			++i;   // truncated or invalid lead byte
			return 0;
		}
		return cp;
	}

}

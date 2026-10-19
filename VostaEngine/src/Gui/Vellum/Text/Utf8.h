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

	// Start byte of the codepoint preceding `i`, which must sit on a boundary (or
	// equal size()). A malformed byte tail still produces a valid index.
	inline std::size_t utf8Prev(const std::string& s, std::size_t i) {
		if (i == 0 || i > s.size())
			return 0;
		--i;
		while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
			--i;
		return i;
	}

	// Appends `cp` as UTF-8. Values outside Unicode become U+FFFD.
	inline void utf8Append(std::string& out, uint32_t cp) {
		if (cp > 0x10FFFF)
			cp = 0xFFFD;
		if (cp < 0x80) {
			out.push_back(static_cast<char>(cp));
		}
		else if (cp < 0x800) {
			out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
		else if (cp < 0x10000) {
			out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
		else {
			out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
	}

}

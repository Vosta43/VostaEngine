#pragma once

#include "Core/Core.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace ve {
	namespace utils {

		// A file's last-write time as a raw tick count, or 0 when it cannot be read
		// (missing, locked, permission denied). Only equality between two reads of the
		// SAME file is meaningful: a view records this when it loads an asset and
		// re-reads it each frame to notice the bytes changing underneath it — from any
		// writer, in this process or another. That is what lets hot-reload work without
		// every writer having to announce itself. Header-inline so the engine and the
		// editor share one definition instead of exporting it across the DLL.
		inline int64_t fileWriteTicks(const std::string& path) {
			if (path.empty()) return 0;
			std::error_code ec;
			const auto t = std::filesystem::last_write_time(path, ec);
			return ec ? 0 : static_cast<int64_t>(t.time_since_epoch().count());
		}

		// Tools to devide engine asset and external asset (.veasset and .png/.pbj/.mp3 etc)
		inline bool isEngineAsset(const std::string& path) {
			size_t dotPos = path.rfind('.');
			if (dotPos == std::string::npos) {
				return false;
			}
			std::string ext = path.substr(dotPos);

			return ext == ".veasset";
		}

		inline std::string getExtension(const std::string& path) {
			size_t dotPos = path.rfind('.');
			if (dotPos == std::string::npos) {
				return "";
			}
			return path.substr(dotPos);
		}

		// Read the type token from a .veasset without loading the file. Both
		// archives length-prefix the token: text writes ASCII digits, binary a
		// little-endian uint32. Used to tell asset kinds apart cheaply.
		inline std::string peekAssetToken(const std::string& path) {
			std::ifstream file(path, std::ios::binary);
			if (!file.is_open()) return {};

			unsigned char first = 0;
			file.read(reinterpret_cast<char*>(&first), 1);
			if (!file) return {};
			file.seekg(0, std::ios::beg);

			uint32_t len = 0;
			if (first >= 0x20 && first < 0x7F) {
				file >> len;   // text: "<len> <token>"
				file.get();    // consume the single space
			}
			else {
				file.read(reinterpret_cast<char*>(&len), sizeof(len));
			}

			if (len == 0 || len > 64) return {};
			std::string token(len, '\0');
			file.read(token.data(), static_cast<std::streamsize>(len));
			return file ? token : std::string();
		}

	}
}
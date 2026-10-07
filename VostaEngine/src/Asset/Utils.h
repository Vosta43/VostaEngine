#pragma once

#include "Core/Core.h"

#include <cstdint>
#include <fstream>
#include <string>

namespace ve {
	namespace utils {
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
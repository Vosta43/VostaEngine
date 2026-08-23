#pragma once

#include "Core/Core.h"

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


	}
}
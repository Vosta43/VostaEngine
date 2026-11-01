#pragma once

#include "Core/Core.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ve {

	// Decoded payload produced on a load worker and installed by upload().
	struct SoundPayload {
		std::vector<float> samples;   // interleaved f32; empty when streamed
		uint32_t sampleRate = 0;
		uint32_t channels = 0;
		double durationSeconds = 0.0;
		bool streamed = false;
		std::string filePath;         // absolute; streamed voices re-open it
	};

	// A playable sound. Short clips arrive as resident f32 PCM; anything past
	// kStreamThresholdSeconds is streamed from disk by the voice instead, so a
	// soundtrack never sits in the residency budget.
	struct VE_API SoundAsset {
		static constexpr double kStreamThresholdSeconds = 10.0;

		std::vector<float> samples;
		uint32_t sampleRate = 0;
		uint32_t channels = 0;
		double durationSeconds = 0.0;
		bool streamed = false;
		std::string filePath;

		static Ref<SoundAsset> create(const std::string& path);
		static Ref<SoundAsset> createPlaceholder();
		static Ref<SoundPayload> load(const std::string& path);
		static Ref<SoundAsset> upload(Ref<SoundPayload> payload);
	};

}

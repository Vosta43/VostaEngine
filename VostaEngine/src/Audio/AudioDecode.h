#pragma once

#include "Core/Core.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ve {

	// Interleaved f32 PCM plus the file's native format. The mixer resamples to
	// the device rate, so decoding never touches the output sample rate.
	struct DecodedAudio {
		std::vector<float> samples;
		uint32_t sampleRate = 0;
		uint32_t channels = 0;
		double durationSeconds = 0.0;

		uint64_t frameCount() const {
			return channels ? (uint64_t)(samples.size() / channels) : 0;
		}
	};

	// Random-access cursor over encoded bytes held in memory. Owned by the
	// caller and kept alive for as long as the decoder uses it.
	struct AudioMemorySource {
		const uint8_t* data = nullptr;
		size_t size = 0;
		size_t position = 0;
	};

	// Reads the header only. False when the file cannot be opened or does not
	// declare a length (headerless MP3 streams and the like).
	bool probeAudioFile(const std::string& absolutePath, double& outDurationSeconds,
		uint32_t& outSampleRate, uint32_t& outChannels);
	bool probeAudioMemory(const void* data, size_t size, double& outDurationSeconds,
		uint32_t& outSampleRate, uint32_t& outChannels);

	// Full decode to f32 PCM. WAV/FLAC/MP3 come from miniaudio; OGG from the
	// stb_vorbis backend, which is also handed to the resource manager so
	// streamed files decode.
	bool decodeAudioFile(const std::string& absolutePath, DecodedAudio& out);
	bool decodeAudioMemory(const void* data, size_t size, DecodedAudio& out);

}

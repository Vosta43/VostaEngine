#include "vepch.h"
#include "Audio/SoundAsset.h"

#include "Audio/AudioDecode.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"

namespace ve {

	Ref<SoundAsset> SoundAsset::create(const std::string& path) {
		Ref<SoundPayload> payload = load(path);
		if (!payload)
			return nullptr;
		return upload(std::move(payload));
	}

	Ref<SoundAsset> SoundAsset::createPlaceholder() {
		return nullptr;
	}

	Ref<SoundPayload> SoundAsset::load(const std::string& path) {
		const std::string absolute = toAbsolute(path);
		auto payload = CreateRef<SoundPayload>();
		payload->filePath = absolute;

		double seconds = 0.0;
		uint32_t sampleRate = 0;
		uint32_t channels = 0;
		const bool probed = probeAudioFile(absolute, seconds, sampleRate, channels);
		if (probed) {
			payload->sampleRate = sampleRate;
			payload->channels = channels;
			payload->durationSeconds = seconds;
			if (seconds > kStreamThresholdSeconds) {
				payload->streamed = true;
				return payload;
			}
		}

		// Short clips are decoded up front. A file whose length cannot be probed
		// (headerless MP3 streams and the like) also lands here: decoding is the
		// only way to learn its duration, and it is already resident afterwards.
		DecodedAudio decoded;
		if (!decodeAudioFile(absolute, decoded)) {
			VE_CORE_ERROR_PRINT("SoundAsset: cannot decode %s", path.c_str());
			return nullptr;
		}

		payload->samples = std::move(decoded.samples);
		payload->sampleRate = decoded.sampleRate;
		payload->channels = decoded.channels;
		payload->durationSeconds = decoded.durationSeconds;
		payload->streamed = false;
		return payload;
	}

	Ref<SoundAsset> SoundAsset::upload(Ref<SoundPayload> payload) {
		if (!payload)
			return nullptr;

		auto asset = CreateRef<SoundAsset>();
		asset->filePath = std::move(payload->filePath);
		asset->sampleRate = payload->sampleRate;
		asset->channels = payload->channels;
		asset->durationSeconds = payload->durationSeconds;
		asset->streamed = payload->streamed;
		asset->samples = std::move(payload->samples);

		if (!asset->streamed && asset->samples.empty())
			return nullptr;
		return asset;
	}

}

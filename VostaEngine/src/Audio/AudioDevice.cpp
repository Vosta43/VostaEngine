#include "vepch.h"
#include "Audio/AudioDevice.h"

#include "Audio/AudioInternal.h"
#include "Audio/SoundAsset.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"

#include <unordered_map>

namespace ve {

	namespace {

		constexpr int kBusCount = 3;

		int busIndex(SoundBus bus) {
			const int index = static_cast<int>(bus);
			return (index >= 0 && index < kBusCount) ? index : 0;
		}

	}

	struct AudioDevice::Impl {
		ma_engine engine{};
		ma_resource_manager resourceManager{};
		ma_sound_group buses[kBusCount]{};
		bool deviceReady = false;
		bool resourceManagerReady = false;
		bool busReady[kBusCount] = { false, false, false };
		bool paused = false;
		float masterVolume = 1.0f;
		float busVolumes[kBusCount] = { 1.0f, 1.0f, 1.0f };
		Vec3 listenerPosition{ 0.0 };

		struct Voice {
			ma_sound sound{};
			ma_audio_buffer_ref buffer{};
			// A packed streamed clip decodes through this cursor over the asset's
			// encoded bytes; both must outlive the sound.
			ma_decoder decoder{};
			AudioMemorySource decoderSource{};
			bool soundReady = false;
			bool bufferReady = false;
			bool decoderReady = false;
			bool spatial = false;
			Vec3 worldPosition{ 0.0 };
			// Pins the PCM a buffer reference reads and the bytes a decoder reads.
			Ref<SoundAsset> asset;
		};

		void uninitVoice(Voice& voice) {
			if (voice.soundReady) {
				ma_sound_stop(&voice.sound);
				ma_sound_uninit(&voice.sound);
				voice.soundReady = false;
			}
			if (voice.decoderReady) {
				ma_decoder_uninit(&voice.decoder);
				voice.decoderReady = false;
			}
			if (voice.bufferReady) {
				ma_audio_buffer_ref_uninit(&voice.buffer);
				voice.bufferReady = false;
			}
			voice.asset.reset();
		}

		void applyVoicePosition(Voice& voice) {
			if (!voice.spatial || !voice.soundReady)
				return;
			// Subtract in the authoritative double domain before narrowing: the
			// delta is what matters, not the absolute world position.
			const glm::vec3 relative = glm::vec3(voice.worldPosition - listenerPosition);
			ma_sound_set_position(&voice.sound, relative.x, relative.y, relative.z);
		}

		std::unordered_map<uint32_t, Scope<Voice>> voices;
		uint32_t nextVoiceId = 1;
	};

	AudioDevice& AudioDevice::get() {
		static AudioDevice instance;
		return instance;
	}

	AudioDevice::AudioDevice() = default;

	AudioDevice::~AudioDevice() {
		shutdown();
	}

	bool AudioDevice::init() {
		if (!m_impl)
			m_impl = CreateScope<Impl>();
		if (m_impl->deviceReady)
			return true;

		ma_resource_manager_config resourceManagerConfig = ma_resource_manager_config_init();
		attachAudioCustomDecoders(resourceManagerConfig);
		if (ma_resource_manager_init(&resourceManagerConfig, &m_impl->resourceManager) == MA_SUCCESS)
			m_impl->resourceManagerReady = true;

		ma_engine_config engineConfig = ma_engine_config_init();
		if (m_impl->resourceManagerReady)
			engineConfig.pResourceManager = &m_impl->resourceManager;

		if (ma_engine_init(&engineConfig, &m_impl->engine) != MA_SUCCESS) {
			VE_CORE_WARN_PRINT("%s", "AudioDevice: no output device available, audio disabled");
			if (m_impl->resourceManagerReady) {
				ma_resource_manager_uninit(&m_impl->resourceManager);
				m_impl->resourceManagerReady = false;
			}
			return false;
		}
		m_impl->deviceReady = true;

		for (int i = 0; i < kBusCount; ++i) {
			if (ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->buses[i]) == MA_SUCCESS) {
				m_impl->busReady[i] = true;
				ma_sound_group_set_volume(&m_impl->buses[i], m_impl->busVolumes[i]);
			}
		}

		ma_engine_set_volume(&m_impl->engine, m_impl->masterVolume);
		ma_engine_listener_set_position(&m_impl->engine, 0, 0.0f, 0.0f, 0.0f);
		ma_engine_listener_set_direction(&m_impl->engine, 0, 0.0f, 0.0f, -1.0f);
		ma_engine_listener_set_world_up(&m_impl->engine, 0, 0.0f, 1.0f, 0.0f);
		return true;
	}

	void AudioDevice::shutdown() {
		if (!m_impl)
			return;

		stopAllVoices();

		for (int i = 0; i < kBusCount; ++i) {
			if (m_impl->busReady[i]) {
				ma_sound_group_uninit(&m_impl->buses[i]);
				m_impl->busReady[i] = false;
			}
		}

		if (m_impl->deviceReady) {
			ma_engine_uninit(&m_impl->engine);
			m_impl->deviceReady = false;
		}
		if (m_impl->resourceManagerReady) {
			ma_resource_manager_uninit(&m_impl->resourceManager);
			m_impl->resourceManagerReady = false;
		}
		m_impl->paused = false;
	}

	bool AudioDevice::isAvailable() const {
		return m_impl && m_impl->deviceReady;
	}

	void AudioDevice::update() {
		if (!m_impl || !m_impl->deviceReady)
			return;

		for (auto it = m_impl->voices.begin(); it != m_impl->voices.end();) {
			Impl::Voice& voice = *it->second;
			// The listener may have moved without the source entity moving.
			m_impl->applyVoicePosition(voice);

			if (ma_sound_at_end(&voice.sound)) {
				m_impl->uninitVoice(voice);
				it = m_impl->voices.erase(it);
			}
			else {
				++it;
			}
		}
	}

	void AudioDevice::setListenerPosition(const Vec3& worldPosition) {
		if (m_impl)
			m_impl->listenerPosition = worldPosition;
	}

	void AudioDevice::setListenerOrientation(const Vec3f& forward, const Vec3f& up) {
		if (!m_impl || !m_impl->deviceReady)
			return;
		ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
		ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
	}

	void AudioDevice::setMasterVolume(float volume) {
		if (!m_impl)
			return;
		m_impl->masterVolume = volume;
		if (m_impl->deviceReady)
			ma_engine_set_volume(&m_impl->engine, volume);
	}

	float AudioDevice::masterVolume() const {
		return m_impl ? m_impl->masterVolume : 1.0f;
	}

	void AudioDevice::setBusVolume(SoundBus bus, float volume) {
		if (!m_impl)
			return;
		const int index = busIndex(bus);
		m_impl->busVolumes[index] = volume;
		if (m_impl->deviceReady && m_impl->busReady[index])
			ma_sound_group_set_volume(&m_impl->buses[index], volume);
	}

	float AudioDevice::busVolume(SoundBus bus) const {
		return m_impl ? m_impl->busVolumes[busIndex(bus)] : 1.0f;
	}

	void AudioDevice::setPaused(bool paused) {
		if (!m_impl || !m_impl->deviceReady || m_impl->paused == paused)
			return;
		m_impl->paused = paused;
		if (paused)
			ma_engine_stop(&m_impl->engine);
		else
			ma_engine_start(&m_impl->engine);
	}

	bool AudioDevice::isPaused() const {
		return m_impl && m_impl->paused;
	}

	uint32_t AudioDevice::playVoice(const AssetHandle& sound, const AudioVoiceParams& params,
		const Vec3& worldPosition) {
		if (!m_impl || !m_impl->deviceReady || !sound.isValid())
			return 0;

		Ref<SoundAsset> asset = ResourceManager::get<SoundAsset>(sound);
		if (!asset)
			return 0;

		auto voice = CreateScope<Impl::Voice>();
		voice->asset = asset;
		voice->spatial = params.spatial;
		voice->worldPosition = worldPosition;

		const int bus = busIndex(params.bus);
		ma_sound_group* group = m_impl->busReady[bus] ? &m_impl->buses[bus] : nullptr;

		ma_result result = MA_ERROR;
		if (asset->streamed) {
			if (asset->encoded && !asset->encoded->empty() &&
				initAudioMemoryDecoder(asset->encoded->data(), asset->encoded->size(),
					voice->decoderSource, &voice->decoder)) {
				voice->decoderReady = true;
				result = ma_sound_init_from_data_source(&m_impl->engine, &voice->decoder.ds,
					0, group, &voice->sound);
			}
			else {
				result = ma_sound_init_from_file(&m_impl->engine, asset->filePath.c_str(),
					MA_SOUND_FLAG_STREAM, group, nullptr, &voice->sound);
			}
		}
		else if (asset->channels > 0 && !asset->samples.empty()) {
			result = ma_audio_buffer_ref_init(ma_format_f32, asset->channels,
				asset->samples.data(), asset->samples.size() / asset->channels, &voice->buffer);
			if (result == MA_SUCCESS) {
				voice->bufferReady = true;
				result = ma_sound_init_from_data_source(&m_impl->engine, &voice->buffer, 0,
					group, &voice->sound);
			}
		}

		if (result != MA_SUCCESS) {
			m_impl->uninitVoice(*voice);
			return 0;
		}
		voice->soundReady = true;

		ma_sound_set_looping(&voice->sound, params.loop ? MA_TRUE : MA_FALSE);
		ma_sound_set_spatialization_enabled(&voice->sound, params.spatial ? MA_TRUE : MA_FALSE);
		ma_sound_set_volume(&voice->sound, params.volume);
		ma_sound_set_pitch(&voice->sound, params.pitch);
		if (params.spatial) {
			ma_sound_set_positioning(&voice->sound, ma_positioning_absolute);
			ma_sound_set_attenuation_model(&voice->sound, ma_attenuation_model_linear);
			ma_sound_set_min_distance(&voice->sound, params.minDistance);
			ma_sound_set_max_distance(&voice->sound, params.maxDistance);
			m_impl->applyVoicePosition(*voice);
		}

		if (ma_sound_start(&voice->sound) != MA_SUCCESS) {
			m_impl->uninitVoice(*voice);
			return 0;
		}

		const uint32_t voiceId = m_impl->nextVoiceId++;
		m_impl->voices.emplace(voiceId, std::move(voice));
		return voiceId;
	}

	uint32_t AudioDevice::playOneShot(const AssetHandle& sound, float volume, float pitch,
		SoundBus bus) {
		AudioVoiceParams params;
		params.bus = bus;
		params.volume = volume;
		params.pitch = pitch;
		params.spatial = false;
		return playVoice(sound, params, Vec3(0.0));
	}

	void AudioDevice::updateVoice(uint32_t voiceId, float volume, float pitch,
		const Vec3& worldPosition) {
		if (!m_impl)
			return;
		auto it = m_impl->voices.find(voiceId);
		if (it == m_impl->voices.end())
			return;

		Impl::Voice& voice = *it->second;
		voice.worldPosition = worldPosition;
		ma_sound_set_volume(&voice.sound, volume);
		ma_sound_set_pitch(&voice.sound, pitch);
		m_impl->applyVoicePosition(voice);
	}

	void AudioDevice::stopVoice(uint32_t voiceId) {
		if (!m_impl)
			return;
		auto it = m_impl->voices.find(voiceId);
		if (it == m_impl->voices.end())
			return;
		m_impl->uninitVoice(*it->second);
		m_impl->voices.erase(it);
	}

	bool AudioDevice::hasVoice(uint32_t voiceId) const {
		return m_impl && m_impl->voices.find(voiceId) != m_impl->voices.end();
	}

	void AudioDevice::stopAllVoices() {
		if (!m_impl)
			return;
		for (auto& [id, voice] : m_impl->voices)
			m_impl->uninitVoice(*voice);
		m_impl->voices.clear();
	}

}

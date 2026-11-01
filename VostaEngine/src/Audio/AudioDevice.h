#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"
#include "Core/Types.h"

#include <cstdint>

namespace ve {

	// Mixer destinations, mirroring UE sound classes / Unity mixer groups: each
	// carries one gain under the master gain.
	enum class SoundBus : uint8_t {
		Sfx = 0,
		Music,
		Ui
	};

	struct AudioVoiceParams {
		SoundBus bus = SoundBus::Sfx;
		float volume = 1.0f;
		float pitch = 1.0f;
		bool loop = false;
		bool spatial = false;
		float minDistance = 1.0f;
		float maxDistance = 10000.0f;
	};

	// Engine-wide mixer: one output engine, three bus groups and the voices it
	// owns. Main-thread only; the pimpl keeps miniaudio out of public headers.
	class VE_API AudioDevice {
	public:
		static AudioDevice& get();

		// Creates the device. False when no output device exists; every later
		// call is a no-op, so headless runs keep working.
		bool init();
		void shutdown();
		bool isAvailable() const;

		// Reaps finished voices. Call once per frame on the main thread.
		void update();

		// The listener is the origin of mixer space: callers pass world
		// positions and the device subtracts this anchor before narrowing.
		void setListenerPosition(const Vec3& worldPosition);
		void setListenerOrientation(const Vec3f& forward, const Vec3f& up);

		void setMasterVolume(float volume);
		float masterVolume() const;
		void setBusVolume(SoundBus bus, float volume);
		float busVolume(SoundBus bus) const;

		// Stops/resumes the output device (play-in-editor).
		void setPaused(bool paused);
		bool isPaused() const;

		// Starts a tracked voice that lives until stopped or finished. Returns 0
		// when the sound is not resident or no device is available.
		uint32_t playVoice(const AssetHandle& sound, const AudioVoiceParams& params,
			const Vec3& worldPosition);

		// Fire-and-forget 2D playback. The voice is still tracked and reaped.
		uint32_t playOneShot(const AssetHandle& sound, float volume = 1.0f,
			float pitch = 1.0f, SoundBus bus = SoundBus::Sfx);

		// Pushes live parameter/position edits into an existing voice.
		void updateVoice(uint32_t voiceId, float volume, float pitch, const Vec3& worldPosition);
		void stopVoice(uint32_t voiceId);
		bool hasVoice(uint32_t voiceId) const;
		void stopAllVoices();

	private:
		AudioDevice();
		~AudioDevice();
		AudioDevice(const AudioDevice&) = delete;
		AudioDevice& operator=(const AudioDevice&) = delete;

		struct Impl;
		Scope<Impl> m_impl;
	};

}

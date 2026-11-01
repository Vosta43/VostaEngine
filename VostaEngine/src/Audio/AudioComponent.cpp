#include "vepch.h"
#include "Scene/Components.h"

#include "Audio/AudioDevice.h"

#include <algorithm>

namespace ve {

	AudioVoiceParams AudioSourceComponent::voiceParams() const {
		AudioVoiceParams params;
		params.bus = static_cast<SoundBus>(std::clamp(bus, 0, 2));
		params.volume = volume;
		params.pitch = pitch;
		params.loop = loop;
		params.spatial = spatial;
		params.minDistance = minDistance;
		params.maxDistance = maxDistance;
		return params;
	}

	void AudioSourceComponent::tick(const Vec3& worldPosition) {
		AudioDevice& device = AudioDevice::get();

		if (m_armed) {
			m_armed = false;
			if (playOnStart)
				m_wantPlay = true;
		}

		// The asset streams in asynchronously; keep retrying until it resolves.
		if (m_wantPlay && soundHandle.isValid()) {
			m_voiceId = device.playVoice(soundHandle, voiceParams(), worldPosition);
			if (m_voiceId != 0)
				m_wantPlay = false;
		}

		if (m_voiceId != 0) {
			if (device.hasVoice(m_voiceId))
				device.updateVoice(m_voiceId, volume, pitch, worldPosition);
			else
				m_voiceId = 0;
		}
	}

	void AudioSourceComponent::play(const Vec3& worldPosition) {
		stop();
		m_wantPlay = true;
		tick(worldPosition);
	}

	void AudioSourceComponent::stop() {
		if (m_voiceId != 0) {
			AudioDevice::get().stopVoice(m_voiceId);
			m_voiceId = 0;
		}
		m_wantPlay = false;
	}

	bool AudioSourceComponent::isPlaying() const {
		return m_voiceId != 0 && AudioDevice::get().hasVoice(m_voiceId);
	}

}

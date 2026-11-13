#pragma once

#include "Core/Core.h"

#include <cstdint>

namespace ve {

	// Client-side model of the server clock. Every snapshot provides one sample
	// of the difference between the two clocks; the offset is low-pass filtered
	// so transport jitter never reaches the render timeline, while a large error
	// (first packet, reconnect) snaps instead of smoothing.
	class VE_API NetClock {
	public:
		void reset();

		// One sample per logical tick, at close. packetTimeMs is the server clock
		// stamp carried by the tick, rttMs the transport round trip and
		// localNowMs this client's monotonic clock.
		void onSnapshot(uint32_t packetTimeMs, uint32_t rttMs, double localNowMs);

		// Advances the render timeline. Returns the server time to sample history
		// at (estimated server now minus the interpolation delay). Never moves
		// backwards, so a clock correction cannot rewind every replicated object.
		double tick(double localNowMs);

		double renderTimeMs() const { return m_renderMs; }
		double offsetMs() const { return m_offsetMs; }
		float delayMs() const { return m_delayMs; }
		void setDelayMs(float delayMs);
		bool started() const { return m_started; }
		uint32_t snapCount() const { return m_snaps; }

	private:
		double m_offsetMs = 0.0;
		double m_renderMs = 0.0;
		double m_lastLocalMs = 0.0;
		float m_delayMs = 100.0f;
		bool m_started = false;
		bool m_haveRender = false;
		uint32_t m_snaps = 0;
	};

}

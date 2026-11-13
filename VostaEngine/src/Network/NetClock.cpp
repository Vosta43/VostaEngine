#include "vepch.h"
#include "Network/NetClock.h"

#include <cmath>

namespace ve {

	namespace {

		// Further than this from the running offset is a different clock, not
		// jitter: first packet, reconnect, or a system time change.
		constexpr double kSnapThresholdMs = 250.0;

		// Time constant of the offset filter. Well above the tick period so jitter
		// averages out, short enough to follow crystal drift (tens of ppm).
		constexpr double kSmoothTimeMs = 300.0;

	}

	void NetClock::reset() {
		m_offsetMs = 0.0;
		m_renderMs = 0.0;
		m_lastLocalMs = 0.0;
		m_delayMs = 100.0f;
		m_started = false;
		m_haveRender = false;
		m_snaps = 0;
	}

	void NetClock::setDelayMs(float delayMs) {
		m_delayMs = delayMs > 0.0f ? delayMs : 0.0f;
	}

	void NetClock::onSnapshot(uint32_t packetTimeMs, uint32_t rttMs, double localNowMs) {
		// The packet left the server half a round trip before it arrived here.
		const double sample = (double)packetTimeMs + 0.5 * (double)rttMs - localNowMs;

		if (!m_started) {
			m_started = true;
			m_offsetMs = sample;
			m_lastLocalMs = localNowMs;
			++m_snaps;
		}
		else {
			const double elapsedMs = localNowMs - m_lastLocalMs;
			m_lastLocalMs = localNowMs;

			const double error = sample - m_offsetMs;
			if (std::fabs(error) > kSnapThresholdMs) {
				m_offsetMs = sample;
				++m_snaps;
			}
			else if (elapsedMs > 0.0) {
				const double blend = elapsedMs / kSmoothTimeMs;
				m_offsetMs += error * (blend < 1.0 ? blend : 1.0);
			}
		}

		tick(localNowMs);
	}

	double NetClock::tick(double localNowMs) {
		const double render = localNowMs + m_offsetMs - (double)m_delayMs;
		if (!m_haveRender || render > m_renderMs) {
			m_renderMs = render;
			m_haveRender = true;
		}
		return m_renderMs;
	}

}

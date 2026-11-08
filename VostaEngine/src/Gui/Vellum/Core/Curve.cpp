#include "vepch.h"
#include "Curve.h"

#include <algorithm>
#include <cmath>

namespace ve::vellum {

	CurveSequence::CurveSequence(float loopPeriod)
		: m_loopPeriod(loopPeriod > 0.0f ? loopPeriod : 0.0f) {}

	int CurveSequence::addCurve(float start, float duration, Ease easing) {
		start = start > 0.0f ? start : 0.0f;
		duration = duration > 0.0f ? duration : 0.0f;
		m_curves.push_back({ start, duration, easing });
		m_length = std::max(m_length, start + duration);
		return static_cast<int>(m_curves.size()) - 1;
	}

	void CurveSequence::restart(double now, float origin, float rate, bool pingPong) {
		m_playStart = now;
		m_origin = origin;
		m_rate = rate;
		m_pingPong = pingPong;
		m_playing = true;
		m_completed = false;
	}

	void CurveSequence::play(double now) {
		restart(now, 0.0f, 1.0f, false);
	}

	void CurveSequence::playReverse(double now) {
		restart(now, m_length, -1.0f, false);
	}

	void CurveSequence::pingPong(double now) {
		restart(now, 0.0f, 1.0f, true);
	}

	void CurveSequence::stop(double now) {
		if (!m_playing)
			return;
		m_origin = localTime(now);   // later value() keeps sampling here
		m_playing = false;
	}

	void CurveSequence::jumpTo(float seconds) {
		m_playing = false;
		m_pingPong = false;
		m_origin = seconds;
	}

	float CurveSequence::localTime(double now) const {
		const double raw = m_playing
			? static_cast<double>(m_origin) + (now - m_playStart) * m_rate
			: static_cast<double>(m_origin);

		if (m_pingPong) {
			// Reflect the second half back down: 0 -> 1 -> 0, over two periods.
			const double period = m_loopPeriod > 0.0f ? m_loopPeriod
				: (m_length > 0.0f ? static_cast<double>(m_length) : 1.0);
			double p = std::fmod(raw, period * 2.0);
			if (p < 0.0)
				p += period * 2.0;
			return static_cast<float>(p <= period ? p : period * 2.0 - p);
		}

		if (m_loopPeriod > 0.0f) {
			double p = std::fmod(raw, static_cast<double>(m_loopPeriod));
			if (p < 0.0)
				p += m_loopPeriod;
			return static_cast<float>(p);
		}

		return static_cast<float>(raw);
	}

	bool CurveSequence::update(double now) {
		if (!m_playing)
			return false;

		// A loop has no end; only one-shots complete.
		if (m_loopPeriod > 0.0f || m_pingPong)
			return true;

		const float t = localTime(now);
		if (m_rate >= 0.0f ? t < m_length : t > 0.0f)
			return true;

		m_origin = m_rate >= 0.0f ? m_length : 0.0f;
		m_playing = false;
		if (!m_completed) {
			m_completed = true;
			if (onComplete)
				onComplete();
		}
		return false;
	}

	float CurveSequence::value(int curve, double now) const {
		if (m_curves.empty())
			return 0.0f;

		const auto index = static_cast<std::size_t>(
			std::clamp(curve, 0, static_cast<int>(m_curves.size()) - 1));
		const Curve& c = m_curves[index];
		const float t = localTime(now);

		if (c.duration <= 0.0f)
			return t >= c.start ? 1.0f : 0.0f;
		return easeValue(c.easing, (t - c.start) / c.duration);
	}

}

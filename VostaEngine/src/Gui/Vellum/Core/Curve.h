#pragma once

#include "Core/Core.h"
#include "Easing.h"

#include <functional>
#include <vector>

namespace ve::vellum {

	// Eased curves on one timeline, sampled from an absolute clock: a skipped frame cannot drift.
	class VE_API CurveSequence {
	public:
		// `loopPeriod` > 0 repeats forever (seconds); 0 plays once.
		explicit CurveSequence(float loopPeriod = 0.0f);

		// `start` and `duration` are seconds from the beginning of the sequence.
		// Returns the index to sample with value().
		int addCurve(float start, float duration, Ease easing = Ease::Linear);

		void play(double now);
		// Starts at the end and runs back to 0.
		void playReverse(double now);
		// 0 -> 1 -> 0, repeating over `loopPeriod` (or the sequence length).
		void pingPong(double now);
		// Freezes at the value it holds now.
		void stop(double now);
		// Samples at `seconds` without playing: a pose, or the end state.
		void jumpTo(float seconds);

		// Advances the clock. Returns true while playing; a one-shot fires
		// onComplete exactly once when it reaches its end.
		bool update(double now);

		bool  playing() const { return m_playing; }
		float length() const { return m_length; }

		// Eased 0..1 for `curve` at `now`; clamped outside the curve's window.
		float value(int curve, double now) const;

		std::function<void()> onComplete;

	private:
		struct Curve {
			float start = 0.0f;
			float duration = 0.0f;
			Ease  easing = Ease::Linear;
		};

		float localTime(double now) const;
		void  restart(double now, float origin, float rate, bool pingPong);

		std::vector<Curve> m_curves;
		double m_playStart = 0.0;   // host clock when playback began
		float  m_origin = 0.0f;     // sequence seconds at m_playStart, or while stopped
		float  m_rate = 1.0f;       // negative plays backwards
		float  m_loopPeriod = 0.0f;
		float  m_length = 0.0f;
		bool   m_playing = false;
		bool   m_pingPong = false;
		bool   m_completed = false;
	};

}

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ve::vellum {

	// Named easing curves for transitions; kept small so authored JSON stays readable.
	enum class Ease : uint8_t {
		Linear,
		QuadIn, QuadOut, QuadInOut,
		CubicIn, CubicOut, CubicInOut,
		BackOut,
	};

	// Eased 0..1. `t` is clamped so callers may pass raw progress.
	inline float easeValue(Ease easing, float t) {
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
		switch (easing) {
			case Ease::QuadIn:     return t * t;
			case Ease::QuadOut:    return 1.0f - (1.0f - t) * (1.0f - t);
			case Ease::QuadInOut:  return t < 0.5f
				? 2.0f * t * t
				: 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
			case Ease::CubicIn:    return t * t * t;
			case Ease::CubicOut: { const float u = 1.0f - t; return 1.0f - u * u * u; }
			case Ease::CubicInOut: return t < 0.5f
				? 4.0f * t * t * t
				: 1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
			case Ease::BackOut: {
				// Overshoots past 1 before settling: the "pop" a panel wants.
				constexpr float c1 = 1.70158f;
				constexpr float c3 = c1 + 1.0f;
				const float u = t - 1.0f;
				return 1.0f + c3 * u * u * u + c1 * u * u;
			}
			case Ease::Linear:
			default:               return t;
		}
	}

	inline const char* easeName(Ease easing) {
		switch (easing) {
			case Ease::QuadIn:     return "quadIn";
			case Ease::QuadOut:    return "quadOut";
			case Ease::QuadInOut:  return "quadInOut";
			case Ease::CubicIn:    return "cubicIn";
			case Ease::CubicOut:   return "cubicOut";
			case Ease::CubicInOut: return "cubicInOut";
			case Ease::BackOut:    return "backOut";
			case Ease::Linear:
			default:               return "linear";
		}
	}

	// Case-insensitive lookup, so authored JSON may use "CubicOut" or "cubicout".
	inline bool parseEase(const std::string& name, Ease& out) {
		static const Ease all[] = {
			Ease::Linear,
			Ease::QuadIn, Ease::QuadOut, Ease::QuadInOut,
			Ease::CubicIn, Ease::CubicOut, Ease::CubicInOut,
			Ease::BackOut,
		};
		for (Ease candidate : all) {
			const char* k = easeName(candidate);
			std::size_t i = 0;
			for (; i < name.size() && k[i] != '\0'; ++i) {
				const char a = name[i];
				const char b = k[i];
				const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
				const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
				if (la != lb)
					break;
			}
			if (i == name.size() && k[i] == '\0') {
				out = candidate;
				return true;
			}
		}
		return false;
	}

}

#pragma once

#include <glm.hpp>

namespace ve::vellum {

// One frame's worth of mouse input, sampled by the host and handed to
// WidgetTree::frame(). Position is polled every frame; the edge flags are
// latched from button events, so a press and release inside the same frame
// still register (a poll would miss it).
struct InputFrame {
	glm::vec2 pos{ 0.0f };
	bool justDown = false;
	bool justUp = false;
	bool buttonDown = false;
};

}

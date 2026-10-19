#pragma once

#include <glm.hpp>
#include <cstdint>
#include <vector>

namespace ve::vellum {

// One keyboard event latched by the host. Kept in arrival order — text and key
// edges interleave (type "a", then backspace), so separate queues would reorder
// the edit.
struct KeyInput {
	enum class Kind { Down, Text };

	Kind     kind = Kind::Down;
	int      key = 0;          // Kind::Down: GLFW key code
	int      repeat = 0;       // Kind::Down: GLFW_REPEAT count, 0 for a fresh press
	uint32_t codepoint = 0;    // Kind::Text: decoded Unicode scalar
};

// One frame's worth of mouse and keyboard input, sampled by the host and handed
// to WidgetTree::frame(). Mouse position is polled every frame; the edge flags
// and keys are latched from events, so a press and release inside the same frame
// still register (a poll would miss it).
struct InputFrame {
	glm::vec2 pos{ 0.0f };
	bool justDown = false;
	bool justUp = false;
	bool buttonDown = false;
	float wheel = 0.0f;
	std::vector<KeyInput> keys;
};

}

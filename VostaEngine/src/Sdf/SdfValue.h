#pragma once

#include <functional>

#include <glm.hpp>

namespace ve {

	// A signed-distance function: one distance per point, negative inside the solid.
	// An SDF *node* hands one of these up instead of a buffer — it captures its inputs'
	// functions plus its own parameters, so the node graph itself is the expression
	// tree and the distance is only ever sampled once, at a Marching Cubes node.
	// Empty (null) means "nothing wired into this pin".
	using SdfFn = std::function<float(const glm::vec3&)>;

	struct SdfValue {
		SdfFn fn;
		bool valid() const { return static_cast<bool>(fn); }
	};

}

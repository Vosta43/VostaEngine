#pragma once

#include <glm.hpp>

namespace ve {

	// Authoritative world-space precision. Flips with the build: entity positions,
	// the render origin and other world-anchored values use these, so a large-world
	// project can hold planet-scale coordinates in double while a small-world
	// project pays nothing for it. The GPU never sees these -- render space is
	// always float32 (the *f aliases below).
#ifdef VE_LARGE_WORLD_COORDINATES
	using Vec3 = glm::dvec3;
	using Vec4 = glm::dvec4;
	using Mat4 = glm::dmat4;
#else
	using Vec3 = glm::vec3;
	using Vec4 = glm::vec4;
	using Mat4 = glm::mat4;
#endif

	// Always float32: anything that lands in a vertex buffer or a uniform, plus
	// rotation and scale. Both are O(1) and carry no precision risk -- only
	// translation grows with the world, so only it needs to be double.
	using Vec2f = glm::vec2;
	using Vec3f = glm::vec3;
	using Vec4f = glm::vec4;
	using Mat4f = glm::mat4;
	using Quatf = glm::quat;

}

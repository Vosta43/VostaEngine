#pragma once

#include "Core/Core.h"

namespace ve {

	class Scene;

	// Deep-copies a scene through an in-memory serializer round-trip: serialize to
	// a JSON string, parse it back, deserialize into a fresh Scene. Reuses the same
	// path a save/load takes, so the copy is by construction "what a save would
	// contain".
	//
	// Runtime-only component state (terrain meshes, material instances, ...) is not
	// carried over; each component's init hook rebuilds it lazily on the copy.
	// Asset handles re-resolve to the same ResourceManager objects, so no GPU
	// resource is duplicated. Entity ids are NOT preserved (the copy packs ids from
	// 0) -- safe only while no component references another entity by id.
	//
	// Returns nullptr if the round-trip fails. The target is always a fresh Scene:
	// deserialize() clears the destination registry first, so passing the source
	// would wipe the authored scene.
	VE_API Ref<Scene> cloneScene(const Ref<Scene>& src);

}

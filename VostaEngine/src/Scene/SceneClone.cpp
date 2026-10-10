#include "vepch.h"
#include "SceneClone.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "Core/Json.h"

namespace ve {

	Ref<Scene> cloneScene(const Ref<Scene>& src) {
		if (!src)
			return nullptr;

		JsonWriter w;
		SceneSerializer(src).serialize(w);

		JsonReader r;
		if (!JsonReader::parse(w.str(), r))
			return nullptr;

		auto dst = CreateRef<Scene>();
		if (!SceneSerializer(dst).deserialize(r))
			return nullptr;

		return dst;
	}

}

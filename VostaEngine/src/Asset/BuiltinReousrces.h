#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"

namespace ve {

	// Engine-owned assets that every project can rely on. They live in memory
	// under a reserved "__builtin_..." key rather than as files on disk, so they
	// never collide with project content and never leave stray .veasset files.
	class VE_API BuiltinResources {
	public:

		// Build and register the built-in assets. Call once at engine startup,
		// before any scene is loaded. Safe to call again (an already-built
		// resource is kept).
		static void init();

		// Solid white, matte, no texture maps — the fallback material for
		// anything that has none assigned (terrain, submeshes, ...).
		static AssetHandle getDefaultMaterial();

		// 1x1 opaque white. Serves as the default for a texture parameter a
		// material declares but no instance has overridden — e.g. a white weight
		// map leaves every layer's mask fully open.
		static AssetHandle getDefaultWhiteTexture();

		// Unit UV sphere, generated rather than imported: indexed vertices with
		// smooth normals, real UVs and analytic tangents. Used as the material
		// preview shape and as the editor's stock sphere. Created on first use —
		// a StaticMesh uploads to the GPU, so this needs a live GL context.
		static AssetHandle getBuiltinSphere();

	};

}

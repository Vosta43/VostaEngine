#include "vepch.h"
#include "ParticleAsset.h"

#include "EmitterAssetIO.h"
#include "EmitterCompiler.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"

namespace ve {

	namespace {

		bool loadCompiled(const std::string& path, EmitterAsset& out) {
			const std::string file = toAbsolute(path);
			std::string error;
			if (!loadEmitterAsset(file, out, &error)) {
				VE_CORE_ERROR_PRINT("ParticleAsset: cannot load %s%s%s", path.c_str(),
					error.empty() ? "" : " - ", error.c_str());
				return false;
			}
			if (!compileEmitter(out)) {
				VE_CORE_ERROR_PRINT("ParticleAsset: %s failed to compile", path.c_str());
				return false;
			}
			return true;
		}

	}

	Ref<ParticleAsset> ParticleAsset::create(const std::string& path) {
		auto asset = CreateRef<ParticleAsset>();
		if (!loadCompiled(path, asset->emitter))
			return nullptr;
		return asset;
	}

	void ParticleAsset::refreshFromFile(const std::string& path) {
		const AssetHandle handle = ResourceManager::find<ParticleAsset>(path);
		if (!handle.isValid())
			return;

		Ref<ParticleAsset> asset = ResourceManager::get<ParticleAsset>(handle);
		if (!asset)
			return;

		EmitterAsset fresh;
		if (!loadCompiled(path, fresh))
			return;

		asset->emitter = std::move(fresh);
		asset->revision++;
	}

}

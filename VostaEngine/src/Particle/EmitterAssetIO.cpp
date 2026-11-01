#include "vepch.h"
#include "EmitterAssetIO.h"

#include "Core/Log.h"
#include "Modules.h"
#include "Scene/Archive.h"

#include <algorithm>
#include <atomic>
#include <vector>

namespace ve {

	namespace {

		// Namespace half of generated GUIDs, so a fresh id never looks like a default 0.
		constexpr uint64_t kGuidNamespace = 0x766F737461706172ull;   // "vostapar"

		uint64_t nextGuidValue() {
			static std::atomic<uint64_t> counter{ 1 };
			return counter.fetch_add(1, std::memory_order_relaxed);
		}

		bool guidIsZero(const Guid16& guid) {
			return guid.a == 0 && guid.b == 0;
		}

		bool guidInUse(const std::vector<Guid16>& used, const Guid16& guid) {
			for (const Guid16& other : used) {
				if (other.a == guid.a && other.b == guid.b)
					return true;
			}
			return false;
		}

		Guid16 makeGuid() {
			Guid16 guid;
			guid.a = nextGuidValue();
			guid.b = kGuidNamespace;
			return guid;
		}

		const ModuleAsset* findBuiltin(const std::string& name) {
			for (const ModuleAsset* module : builtin::builtinModules()) {
				if (module->name == name)
					return module;
			}
			return nullptr;
		}

		const char* stageLabel(Stage stage) {
			switch (stage) {
			case Stage::EmitterSpawn:   return "EmitterSpawn";
			case Stage::EmitterUpdate:  return "EmitterUpdate";
			case Stage::ParticleSpawn:  return "ParticleSpawn";
			case Stage::ParticleUpdate: return "ParticleUpdate";
			}
			return "Unknown";
		}

		uint16_t paramWidth(const ModuleAsset& module) {
			uint16_t width = 0;
			for (const ModuleParamDecl& decl : module.params)
				width = static_cast<uint16_t>(width + decl.comps);
			return width;
		}

		void fillDeclDefaults(const ModuleAsset& module, std::vector<float>& params) {
			params.assign(paramWidth(module), 0.0f);
			uint16_t offset = 0;
			for (const ModuleParamDecl& decl : module.params) {
				for (uint8_t c = 0; c < decl.comps; ++c)
					params[offset + c] = decl.value[c];
				offset = static_cast<uint16_t>(offset + decl.comps);
			}
		}

		void writeGuid(Archive& ar, const Guid16& guid) {
			ar << static_cast<int32_t>(guid.a & 0xFFFFFFFFull);
			ar << static_cast<int32_t>(guid.a >> 32);
			ar << static_cast<int32_t>(guid.b & 0xFFFFFFFFull);
			ar << static_cast<int32_t>(guid.b >> 32);
		}

		Guid16 readGuid(Archive& ar) {
			int32_t aLo = 0, aHi = 0, bLo = 0, bHi = 0;
			ar >> aLo >> aHi >> bLo >> bHi;
			Guid16 guid;
			guid.a = static_cast<uint64_t>(static_cast<uint32_t>(aLo)) |
				(static_cast<uint64_t>(static_cast<uint32_t>(aHi)) << 32);
			guid.b = static_cast<uint64_t>(static_cast<uint32_t>(bLo)) |
				(static_cast<uint64_t>(static_cast<uint32_t>(bHi)) << 32);
			return guid;
		}

		struct SavedParam {
			std::string name;
			uint8_t comps = 1;
			float values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		};

		void setError(std::string* error, const std::string& text) {
			if (error)
				*error = text;
		}

	}

	ModuleEntry createModuleEntry(const ModuleAsset& module) {
		ModuleEntry entry;
		entry.module = &module;
		entry.guid = makeGuid();
		fillDeclDefaults(module, entry.params);
		return entry;
	}

	void ensureUniqueModuleGuids(EmitterAsset& asset) {
		std::vector<Guid16> used;
		for (uint32_t s = 0; s < kStageCount; ++s) {
			for (ModuleEntry& entry : asset.stages[s]) {
				if (guidIsZero(entry.guid) || guidInUse(used, entry.guid)) {
					Guid16 guid;
					do {
						guid = makeGuid();
					} while (guidInUse(used, guid));
					entry.guid = guid;
				}
				used.push_back(entry.guid);
			}
		}
	}

	EmitterAsset makeDefaultEmitterAsset() {
		EmitterAsset asset;
		asset.stages[static_cast<uint32_t>(Stage::EmitterSpawn)].push_back(createModuleEntry(builtin::initEmitter()));
		asset.stages[static_cast<uint32_t>(Stage::EmitterUpdate)].push_back(createModuleEntry(builtin::spawnRate()));
		asset.stages[static_cast<uint32_t>(Stage::ParticleSpawn)].push_back(createModuleEntry(builtin::initParticle()));
		asset.stages[static_cast<uint32_t>(Stage::ParticleUpdate)].push_back(createModuleEntry(builtin::gravity()));
		asset.stages[static_cast<uint32_t>(Stage::ParticleUpdate)].push_back(createModuleEntry(builtin::lifetime()));
		return asset;
	}

	bool saveEmitterAsset(const EmitterAsset& asset, const std::string& path) {
		TextArchive ar(path, ArchiveMode::write);
		if (!ar.isGood()) {
			VE_CORE_WARN_PRINT("Particle save failed: %s", path.c_str());
			return false;
		}

		ar << std::string("particle");
		ar << kEmitterFormatVersion;

		for (uint32_t s = 0; s < kStageCount; ++s) {
			ar << static_cast<int32_t>(asset.stages[s].size());
			for (const ModuleEntry& entry : asset.stages[s]) {
				const ModuleAsset* module = entry.module;
				ar << std::string(module ? module->name : std::string());
				ar << static_cast<int32_t>(module ? module->version : 0);
				writeGuid(ar, entry.guid);

				const int32_t paramCount = module ? static_cast<int32_t>(module->params.size()) : 0;
				ar << paramCount;
				if (!module)
					continue;

				for (const ModuleParamDecl& decl : module->params) {
					ar << decl.name;
					ar << static_cast<int32_t>(decl.comps);
					const uint16_t base = paramComponentOffset(*module, decl.name);
					for (uint8_t c = 0; c < decl.comps; ++c) {
						const size_t index = static_cast<size_t>(base) + c;
						const float value = index < entry.params.size() ? entry.params[index] : decl.value[c];
						ar << value;
					}
				}
			}
		}

		ar << static_cast<int32_t>(asset.renderer.type);
		ar << asset.renderer.spriteTexturePath;
		ar << static_cast<int32_t>(asset.renderer.subImageSizeX);
		ar << static_cast<int32_t>(asset.renderer.subImageSizeY);

		return true;
	}

	bool loadEmitterAsset(const std::string& path, EmitterAsset& out,
		std::string* error, std::string* warning, bool* unsupportedVersion) {
		setError(error, "");
		if (warning)
			warning->clear();
		if (unsupportedVersion)
			*unsupportedVersion = false;

		TextArchive ar(path, ArchiveMode::read);
		if (!ar.isGood()) {
			setError(error, "cannot open particle asset: " + path);
			return false;
		}

		std::string token;
		ar >> token;
		if (token != "particle") {
			setError(error, "not a particle asset (token '" + token + "')");
			return false;
		}

		int32_t version = 0;
		ar >> version;
		if (version < 1 || version > kEmitterFormatVersion) {
			if (version > kEmitterFormatVersion && unsupportedVersion)
				*unsupportedVersion = true;
			setError(error, "unsupported particle asset version " + std::to_string(version));
			return false;
		}

		EmitterAsset asset;
		std::string warn;

		for (uint32_t s = 0; s < kStageCount; ++s) {
			const Stage stage = static_cast<Stage>(s);
			int32_t count = 0;
			ar >> count;
			if (count < 0 || count > 4096) {
				setError(error, "bad module count in " + std::string(stageLabel(stage)));
				return false;
			}

			for (int32_t i = 0; i < count; ++i) {
				std::string name;
				int32_t moduleVersion = 0;
				ar >> name;
				ar >> moduleVersion;
				const Guid16 guid = readGuid(ar);

				int32_t paramCount = 0;
				ar >> paramCount;
				if (paramCount < 0 || paramCount > 64) {
					setError(error, "bad parameter count for module '" + name + "'");
					return false;
				}

				std::vector<SavedParam> saved;
				saved.reserve(static_cast<size_t>(paramCount));
				for (int32_t p = 0; p < paramCount; ++p) {
					SavedParam param;
					ar >> param.name;
					int32_t comps = 1;
					ar >> comps;
					param.comps = static_cast<uint8_t>(std::clamp(comps, 1, 4));
					for (uint8_t c = 0; c < param.comps; ++c)
						ar >> param.values[c];
					saved.push_back(std::move(param));
				}

				const ModuleAsset* module = findBuiltin(name);
				if (!module) {
					VE_CORE_WARN_PRINT("Particle: unknown module '%s' in %s skipped", name.c_str(), stageLabel(stage));
					warn += "unknown module '" + name + "' skipped; ";
					continue;
				}
				if (static_cast<uint32_t>(moduleVersion) != module->version) {
					VE_CORE_WARN_PRINT("Particle: module '%s' version %d != %u, skipped",
						name.c_str(), moduleVersion, module->version);
					warn += "module '" + name + "' has an unsupported version; ";
					continue;
				}
				if ((module->validStages & stageBit(stage)) == 0) {
					VE_CORE_WARN_PRINT("Particle: module '%s' cannot run in %s, skipped",
						name.c_str(), stageLabel(stage));
					warn += "module '" + name + "' skipped (wrong stage); ";
					continue;
				}

				ModuleEntry entry;
				entry.module = module;
				entry.guid = guid;
				fillDeclDefaults(*module, entry.params);

				for (const SavedParam& param : saved) {
					const ModuleParamDecl* decl = findParamDecl(*module, param.name);
					if (!decl)
						continue;
					const uint16_t base = paramComponentOffset(*module, param.name);
					const uint8_t comps = std::min(param.comps, decl->comps);
					for (uint8_t c = 0; c < comps; ++c) {
						const size_t index = static_cast<size_t>(base) + c;
						if (index < entry.params.size())
							entry.params[index] = param.values[c];
					}
				}

				asset.stages[s].push_back(std::move(entry));
			}
		}

		if (version >= 2) {
			int32_t rendererType = 0;
			ar >> rendererType;
			ar >> asset.renderer.spriteTexturePath;
			int32_t cols = 1;
			int32_t rows = 1;
			ar >> cols >> rows;

			asset.renderer.type = ParticleRendererType::Sprite;
			if (rendererType != static_cast<int32_t>(ParticleRendererType::Sprite))
				warn += "unknown renderer type " + std::to_string(rendererType) + "; ";
			asset.renderer.subImageSizeX = static_cast<uint32_t>(std::clamp(cols, 1, 64));
			asset.renderer.subImageSizeY = static_cast<uint32_t>(std::clamp(rows, 1, 64));
		}

		ensureUniqueModuleGuids(asset);
		out = std::move(asset);
		if (warning)
			*warning = warn;
		return true;
	}

	bool writeDefaultEmitterAsset(const std::string& path) {
		return saveEmitterAsset(makeDefaultEmitterAsset(), path);
	}

}

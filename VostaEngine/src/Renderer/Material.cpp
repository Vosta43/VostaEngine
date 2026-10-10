#include "vepch.h"
#include "Material.h"
#include "SingleMaterial.h"
#include "LayeredMaterial.h"
#include "MaterialNodes.h"
#include "Core/Log.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"

namespace ve {

	namespace {

		std::string withVeassetExt(const std::string& path) {
			if (path.size() < 8 || path.compare(path.size() - 8, 8, ".veasset") != 0)
				return path + ".veasset";
			return path;
		}

		// Every material kind must be listed here. An unknown tag loads as null
		// rather than being parsed as some other kind's body.
		Ref<Material> makeByTag(const std::string& tag) {
			if (tag == SingleMaterial::kTypeTag)  return CreateRef<SingleMaterial>();
			if (tag == LayeredMaterial::kTypeTag) return CreateRef<LayeredMaterial>();
			return nullptr;
		}

	} // namespace

	Ref<Material> Material::create(const std::string& path) {
		const std::string filePath = withVeassetExt(path);

		// Peek the type tag first: the kind decides how the rest is parsed.
		bool exists = false;
		std::string tag;
		{
			TextArchive ar(filePath, ArchiveMode::read);
			exists = ar.isGood();
			if (exists) ar >> tag;
		}

		if (exists) {
			Ref<Material> material = makeByTag(tag);
			if (!material) {
				VE_CORE_WARN_PRINT("Material: unknown type tag '%s' in '%s'",
					tag.c_str(), filePath.c_str());
				return nullptr;
			}
			TextArchive ar(filePath, ArchiveMode::read);
			if (ar.isGood()) material->deserialize(ar);
			// The name in the body can be stale (the asset was renamed after it was
			// written), so the real load path wins.
			material->setAssetPath(toRelative(filePath));
			return material;
		}

		// Fresh material on disk: a single-surface PBR whose lone output node
		// keeps compile() in PBR-fallback mode.
		auto material = CreateRef<SingleMaterial>();
		material->setAssetPath(toRelative(filePath));
		material->graph.addNode(CreateRef<MaterialOutputNode>());
		{
			TextArchive ar(filePath, ArchiveMode::write);
			if (ar.isGood()) {
				material->serialize(ar);
			}
		}
		return material;
	}

	bool Material::refreshFromFile(const std::string& path) {
		const AssetHandle handle = ResourceManager::find<Material>(path);
		if (!handle.isValid())
			return false;

		Ref<Material> material = ResourceManager::get<Material>(handle);
		if (!material)
			return false;

		TextArchive ar(toAbsolute(path), ArchiveMode::read);
		if (!ar.isGood())
			return false;

		// deserialize replaces the graph, re-loads the maps and, when the file says
		// the material was compiled, rebuilds the GL shader (see SingleMaterial).
		material->deserialize(ar);
		// The body's name can be stale after a rename; the registry path wins, same
		// rule create() applies.
		material->setAssetPath(toRelative(toAbsolute(path)));
		return true;
	}

}

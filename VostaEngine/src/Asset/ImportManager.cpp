#include "vepch.h"
#include "ImportManager.h"
#include "StaticMeshImporter.h"
#include "TextureImporter.h"
#include "Core/ResourceManager.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Texture.h"

namespace ve{

	void ImportManager::import(const std::string& path){
		
		std::string ext = utils::getExtension(path);

		if (ext == ".obj") {
			ResourceManager::store<StaticMesh>(path);
		}
		else if (ext == ".png" || ext == ".hdr") {
			ResourceManager::store<Texture2D>(path);
		}
		else if (ext == ".veasset") {
			
		}
		else {

		}

	}
}


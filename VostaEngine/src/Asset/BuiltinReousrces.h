#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"

namespace ve {

	class BuiltinResources {
	public:
		
		static AssetHandle getDefaultMaterial() { return m_defaultMaterialHandle; }


	private:
		static AssetHandle m_defaultMaterialHandle;
	};

}
#include "vepch.h"
#include "StorageBuffer.h"
#include "Platform/OpenGL/OpenGLStorageBuffer.h"
#include "Core/Core.h"

namespace ve {

	Ref<StorageBuffer> StorageBuffer::create(size_t size, const void* data){
		
		// TODO: SWH API
		return CreateRef<OpenGLStorageBuffer>(size,data);
	}
}

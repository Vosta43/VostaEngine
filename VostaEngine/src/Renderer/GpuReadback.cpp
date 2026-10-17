#include "vepch.h"
#include "GpuReadback.h"

#include "Platform/OpenGL/OpenGLGpuReadback.h"

namespace ve {

	Ref<GpuReadback> GpuReadback::create(size_t maxBytes) {
		// TODO: switch the graphicAPI. But it is not an important job yet
		return CreateRef<OpenGLGpuReadback>(maxBytes);
	}

}

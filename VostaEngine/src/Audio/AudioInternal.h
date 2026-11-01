#pragma once

// Engine-internal miniaudio surface. Public headers stay free of it; only the
// audio device and the decoding backend include this.

#include "miniaudio.h"

namespace ve {

	// Registers the custom decoders (OGG/Vorbis) with the resource manager, so
	// streamed files take the same path as pre-decoded ones.
	void attachAudioCustomDecoders(ma_resource_manager_config& config);

}

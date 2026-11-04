#pragma once

// Engine-internal miniaudio surface. Public headers stay free of it; only the
// audio device and the decoding backend include this.

#include "miniaudio.h"

#include "Audio/AudioDecode.h"

namespace ve {

	// Registers the custom decoders (OGG/Vorbis) with the resource manager, so
	// streamed files take the same path as pre-decoded ones.
	void attachAudioCustomDecoders(ma_resource_manager_config& config);

	// Opens a decoder over encoded bytes held in memory; `source` must outlive
	// it. The callback path is used rather than ma_decoder_init_memory because
	// the OGG backend only implements the file and callback initialisers.
	bool initAudioMemoryDecoder(const void* data, size_t size, AudioMemorySource& source,
		ma_decoder* decoder);

}

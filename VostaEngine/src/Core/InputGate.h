#pragma once

#include "Core.h"

namespace ve {

	// True while input reads must report nothing. Several worlds run in one
	// process during multi-world PIE, but there is one keyboard: a world that is
	// not focused ticks inside a ScopedInputBlock, so a game module reading Input
	// there sees no keys instead of another world's keystrokes.
	VE_API bool inputBlocked();

	class VE_API ScopedInputBlock {
	public:
		ScopedInputBlock();
		~ScopedInputBlock();

		ScopedInputBlock(const ScopedInputBlock&) = delete;
		ScopedInputBlock& operator=(const ScopedInputBlock&) = delete;
	};

}

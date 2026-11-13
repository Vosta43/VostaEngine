#include "vepch.h"
#include "Core/InputGate.h"

namespace ve {

	namespace {
		thread_local int g_blockDepth = 0;
	}

	bool inputBlocked() {
		return g_blockDepth > 0;
	}

	ScopedInputBlock::ScopedInputBlock() {
		++g_blockDepth;
	}

	ScopedInputBlock::~ScopedInputBlock() {
		--g_blockDepth;
	}

}

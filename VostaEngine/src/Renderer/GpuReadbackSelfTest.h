#pragma once

namespace ve {

	// TEMPORARY. One-shot check that the image-write -> async-readback path works
	// end to end: it draws a few hundred fragments that imageAtomicAdd into an
	// R32UI counter texture, then reads the counters back. Delete this file, its
	// call in SceneViewRenderer, and its three project entries once the path is
	// trusted.
	//
	// Returns true on the call that ran, since it renders into its own small target
	// and the caller has to put the viewport back.
	class GpuReadbackSelfTest {
	public:
		static bool runOnce();
	};

}

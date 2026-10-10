#pragma once

namespace ve::vellum {

// What a widget returns from a mouse handler. `handled` stops the event from
// bubbling to the parent; `capture` additionally routes every later move/up to
// this widget until the button is released (Slate's FReply::CaptureMouse).
struct Reply {
	bool handled = false;
	bool capture = false;

	static Reply unhandled()        { return { false, false }; }
	static Reply handle()           { return { true,  false }; }
	static Reply handleAndCapture() { return { true,  true  }; }
};

}

#pragma once

namespace ve::vellum {

// What a widget returns from a mouse handler. `handled` stops the event from
// bubbling to the parent; `capture` additionally routes every later move/up to
// this widget until the button is released (Slate's FReply::CaptureMouse);
// `focus` asks the router to move keyboard focus here.
struct Reply {
	bool handled = false;
	bool capture = false;
	bool focus   = false;

	static Reply unhandled()        { return { false, false, false }; }
	static Reply handle()           { return { true,  false, false }; }
	static Reply handleAndCapture() { return { true,  true,  false }; }
	static Reply handleFocus()      { return { true,  false, true  }; }
};

}

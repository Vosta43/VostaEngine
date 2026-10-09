#pragma once

namespace ve {

	class EditorLayer;

	// The editor-side MCP adapter: the ONLY place editor code touches the tool layer.
	// It binds EditorLayer's ordinary, MCP-agnostic operations (its current scene, its
	// save) to the command plane and owns the wire-format details -- the scene
	// provider, the command definitions, the {"error":...} envelopes, the dispatch
	// queue. EditorLayer itself keeps no protocol vocabulary; it only forwards
	// attach / detach / pump.
	namespace EditorMcpTools {

		// Binds the layer: installs the scene provider and registers the editor's
		// commands. Call from EditorLayer::onAttach.
		void attach(EditorLayer& layer);

		// Drops everything attach() bound, so no handler outlives the layer. Call
		// from EditorLayer::onDetach, after the agent has been shut down.
		void detach();

		// Runs queued tool calls on the render thread. Call at the top of the frame
		// from EditorLayer::onUpdate, before any early-out.
		void pump();
	}

}

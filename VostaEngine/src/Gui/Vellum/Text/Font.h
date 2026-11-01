#pragma once

#include "Core/Core.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct stbtt_fontinfo;   // global tag, matches stb_truetype.h

namespace ve::vellum {

	class FontAtlas;
	class VellumRenderer;

	// A font file loaded once per path. `bytes` is shared by every Font that names
	// the file; stb keeps pointers into it, so the face and its bytes live and die
	// together. The face info is size-independent — only the scale changes.
	struct VE_API FaceData {
		std::shared_ptr<const std::vector<unsigned char>> bytes;
		stbtt_fontinfo* info = nullptr;   // owned; deleted by ~FaceData
		~FaceData();
	};

	// One typeface: an ordered face chain plus the per-size atlases it hands out.
	// Faces are consulted in order per codepoint, so a Latin face can lead and a
	// CJK face fall back behind it. Glyphs themselves live in the renderer's one
	// shared GlyphAtlas; an atlas here is only the metrics for one pixel size.
	// Creation needs a live renderer, which must outlive the Font.
	class VE_API Font {
	public:
		// The engine's default UI size, requested by FontManager on startup.
		static constexpr float kReferenceSize = 32.0f;

		// Unreadable faces are skipped; null only when none load.
		static Ref<Font> load(VellumRenderer& renderer, std::vector<std::string> faces,
		                      std::string name = {});
		static Ref<Font> load(VellumRenderer& renderer, std::string face,
		                      std::string name = {});

		const std::string& name() const { return m_name; }

		// The chain as requested, and the faces that actually loaded, in order.
		const std::vector<std::string>& faces() const { return m_faces; }
		const std::vector<std::shared_ptr<const FaceData>>& faceData() const { return m_faceData; }

		// The chain at `pixelSize`; the handle is cached and carries no texture of
		// its own. Null only when this Font has no renderer.
		FontAtlas* at(float pixelSize);

		// Identity used to key glyphs in the shared atlas. A counter rather than
		// the address, so a freed Font's slot cannot be mistaken for a new one.
		uint64_t identity() const { return m_identity; }

	private:
		friend class FontAtlas;

		Font(VellumRenderer& renderer, std::vector<std::string> faces, std::string name);

		VellumRenderer* m_renderer = nullptr;
		std::vector<std::string> m_faces;
		std::vector<std::shared_ptr<const FaceData>> m_faceData;
		std::string m_name;
		uint64_t m_identity = 0;
		std::map<float, Ref<FontAtlas>> m_cache;
	};

}

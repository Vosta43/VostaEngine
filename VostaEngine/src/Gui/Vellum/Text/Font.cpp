#include "vepch.h"
#include "Font.h"

#include "FontAtlas.h"
#include "Core/Log.h"
#include "Gui/Vellum/VellumRenderer.h"

#include "stb_truetype.h"

#include <fstream>
#include <unordered_map>
#include <utility>

namespace ve::vellum {

	namespace {

		// Loaded once per path for the life of the process (weak, so the bytes are
		// released when no Font still references them). This is the piece that keeps
		// a 10 MB CJK face from being copied once per pixel size.
		std::unordered_map<std::string, std::weak_ptr<const FaceData>>& faceCache() {
			static std::unordered_map<std::string, std::weak_ptr<const FaceData>> cache;
			return cache;
		}

		uint64_t nextIdentity() {
			static uint64_t next = 1;
			return next++;
		}

		std::shared_ptr<const FaceData> acquireFace(const std::string& path) {
			if (const auto found = faceCache().find(path); found != faceCache().end()) {
				if (std::shared_ptr<const FaceData> live = found->second.lock())
					return live;
			}

			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file) {
				VE_CORE_ERROR_PRINT("Vellum: font not found: %s", path.c_str());
				return nullptr;
			}
			const std::streamsize size = file.tellg();
			file.seekg(0, std::ios::beg);

			std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
			if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
				VE_CORE_ERROR_PRINT("Vellum: failed to read font: %s", path.c_str());
				return nullptr;
			}

			auto face = std::make_shared<FaceData>();
			face->bytes = std::make_shared<const std::vector<unsigned char>>(std::move(bytes));
			face->info = new stbtt_fontinfo();

			// stb points into `bytes`; moving the shared block above is what keeps
			// that pointer valid for the life of the face.
			if (!stbtt_InitFont(face->info, face->bytes->data(), 0)) {
				VE_CORE_ERROR_PRINT("Vellum: not a valid TrueType font: %s", path.c_str());
				return nullptr;
			}

			faceCache()[path] = face;
			return face;
		}

	}

	FaceData::~FaceData() {
		delete info;
	}

	Font::Font(VellumRenderer& renderer, std::vector<std::string> faces, std::string name)
		: m_renderer(&renderer)
		, m_faces(std::move(faces))
		, m_name(std::move(name))
		, m_identity(nextIdentity()) {
	}

	Ref<Font> Font::load(VellumRenderer& renderer, std::string face, std::string name) {
		return load(renderer, std::vector<std::string>{ std::move(face) }, std::move(name));
	}

	Ref<Font> Font::load(VellumRenderer& renderer, std::vector<std::string> faces,
	                     std::string name) {
		if (faces.empty()) {
			VE_CORE_ERROR_PRINT("Vellum: font '%s' has no face in its chain", name.c_str());
			return nullptr;
		}

		Ref<Font> font(new Font(renderer, std::move(faces), std::move(name)));

		// Loading the faces is the validation: whatever read and initialised is the
		// chain, and an empty result means nothing was usable.
		for (const std::string& path : font->m_faces) {
			if (std::shared_ptr<const FaceData> face = acquireFace(path))
				font->m_faceData.push_back(std::move(face));
		}

		if (font->m_faceData.empty()) {
			VE_CORE_ERROR_PRINT("Vellum: font '%s' has no usable face", font->m_name.c_str());
			return nullptr;
		}
		return font;
	}

	FontAtlas* Font::at(float pixelSize) {
		if (!m_renderer)
			return nullptr;

		const auto found = m_cache.find(pixelSize);
		if (found != m_cache.end())
			return found->second.get();

		Ref<FontAtlas> atlas(new FontAtlas(*this, pixelSize));
		return m_cache.emplace(pixelSize, std::move(atlas)).first->second.get();
	}

}

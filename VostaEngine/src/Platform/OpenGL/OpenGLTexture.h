#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Asset/TextureResource.h"

namespace ve {
	class OpenGLTexture2D : public Texture2D {
	public:
		OpenGLTexture2D(const std::string& path); // Be ready to deprecated
		OpenGLTexture2D(uint32_t width, uint32_t height);
		OpenGLTexture2D(Ref<TextureResource> textureResource);
		OpenGLTexture2D(uint32_t rendererID, uint32_t width, uint32_t height, bool ownsTexture);
		// Creates an empty 2D texture with the given format (e.g. TextureFormat::RG16F).
		OpenGLTexture2D(uint32_t width, uint32_t height, TextureFormat format);
		~OpenGLTexture2D();

		uint32_t getWidth() const override {return m_width;}
		uint32_t getHeight() const override { return m_height; }

		const std::string& getTextureFilePath() const override {return m_path;}

		void setData(void* data,uint32_t size) override;
		void setSubData(const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;

		void bind(uint32_t slot = 0) const override;
		uint32_t getRendererID() const override { return m_rendererId; }
	private:
		uint32_t m_width;
		uint32_t m_height;

		bool m_ownsTexture;

		std::string m_path;
		uint32_t m_rendererId;
		// Format the texture was created with; setData() uses it to pick the
		// correct data format / component type instead of assuming RGBA8.
		TextureFormat m_format;
	};


	class OpenGLTextureCube : public TextureCubeMap {
	public:
		// Use normal texture 2d to create texture cube map
		OpenGLTextureCube(Ref<TextureCubeMapResource> textureResource);
		// Creates an empty RGBA16F cubemap with the given face size, suitable as a render target.
		OpenGLTextureCube(uint32_t faceSize);
		// Creates an empty RGBA16F cubemap with the given face size and mip level count.
		OpenGLTextureCube(uint32_t faceSize, uint32_t mipLevels);
		~OpenGLTextureCube();

		uint32_t getWidth() const override { return m_width; }
		uint32_t getHeight() const override { return m_height; }

		const std::string& getTextureFilePath() const override { return m_path; }
		void setData(void* data, uint32_t size) override;

		void bind(uint32_t slot = 0) const override;
		uint32_t getRendererID() const override { return m_rendererId; }
	private:
		uint32_t m_width;
		uint32_t m_height;

		std::string m_path;
		uint32_t m_rendererId;
	};


	class OpenGLTexture3D : public Texture3D {
	public:
		// Uploads `data` (width*height*depth floats) as a single-channel R16F
		// 3D texture. Linear filter, repeat wrap on all axes, no mipmaps.
		OpenGLTexture3D(uint32_t width, uint32_t height, uint32_t depth, const float* data);
		// Uploads `data` as a 3D texture in the given float format (e.g. RGBA16F
		// for precomputed scattering LUTs). Linear filter, no mipmaps. Wrap mode
		// is clamp-to-edge by default (keeps boundary texels from being blurred
		// during LUT lookups) or repeat (periodic noise volumes) when repeatWrap.
		OpenGLTexture3D(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, const float* data, bool repeatWrap = false);
		~OpenGLTexture3D();

		uint32_t getWidth() const override { return m_width; }
		uint32_t getHeight() const override { return m_height; }
		uint32_t getDepth() const override { return m_depth; }

		const std::string& getTextureFilePath() const override { return m_path; }
		void setData(void* data, uint32_t size) override;

		void bind(uint32_t slot = 0) const override;
		uint32_t getRendererID() const override { return m_rendererId; }
	private:
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_depth;

		std::string m_path;
		uint32_t m_rendererId;
		// Format the texture was created with; setData() uses it to pick the
		// correct data format / component type instead of assuming R16F.
		TextureFormat m_format;
		bool m_repeatWrap;
	};

}

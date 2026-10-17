#include "vepch.h"
#include "OpenGLGpuReadback.h"

#include "Core/Log.h"

#include <glad/glad.h>

#include <cstring>

namespace ve {

	namespace {

		// How glGetTextureSubImage is asked for a texture's texels: the client-side
		// pixel format and component type, plus the bytes one component takes in the
		// destination. Half-float formats are read as 32-bit floats, so their
		// destination component is 4 bytes wide, not 2.
		struct ReadbackLayout {
			GLenum format;
			GLenum type;
			size_t componentBytes;
		};

		bool readbackLayout(TextureFormat tf, ReadbackLayout& out) {
			switch (tf) {
			case TextureFormat::R8:           out = { GL_RED,         GL_UNSIGNED_BYTE,  1 }; return true;
			case TextureFormat::R16F:         out = { GL_RED,         GL_FLOAT,          4 }; return true;
			case TextureFormat::R16UI:        out = { GL_RED_INTEGER, GL_UNSIGNED_SHORT, 2 }; return true;
			case TextureFormat::R32UI:        out = { GL_RED_INTEGER, GL_UNSIGNED_INT,   4 }; return true;
			case TextureFormat::RG16F:        out = { GL_RG,          GL_FLOAT,          4 }; return true;
			case TextureFormat::RGB16F:       out = { GL_RGB,         GL_FLOAT,          4 }; return true;
			case TextureFormat::RGBA16F:      out = { GL_RGBA,        GL_FLOAT,          4 }; return true;
			case TextureFormat::SRGB8:        out = { GL_RGB,         GL_UNSIGNED_BYTE,  1 }; return true;
			case TextureFormat::SRGB8_ALPHA8: out = { GL_RGBA,        GL_UNSIGNED_BYTE,  1 }; return true;
			case TextureFormat::RGB:          out = { GL_RGB,         GL_UNSIGNED_BYTE,  1 }; return true;
			case TextureFormat::RGBA:         out = { GL_RGBA,        GL_UNSIGNED_BYTE,  1 }; return true;
			default:                          return false;
			}
		}

	}

	OpenGLGpuReadback::OpenGLGpuReadback(size_t maxBytes) {
		if (maxBytes == 0) return;
		m_maxBytes = maxBytes;

		glGenBuffers(kSlots, m_pbo);
		for (int i = 0; i < kSlots; ++i) {
			glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[i]);
			glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)maxBytes, nullptr, GL_STREAM_READ);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	}

	OpenGLGpuReadback::~OpenGLGpuReadback() {
		release();
	}

	void OpenGLGpuReadback::release() {
		for (int i = 0; i < kSlots; ++i) {
			if (m_fence[i]) {
				glDeleteSync((GLsync)m_fence[i]);
				m_fence[i] = nullptr;
			}
			if (m_pbo[i]) {
				glDeleteBuffers(1, &m_pbo[i]);
				m_pbo[i] = 0;
			}
			m_issued[i] = 0;
		}
		m_issueCount = 0;
		m_maxBytes = 0;
	}

	bool OpenGLGpuReadback::request(const Ref<Texture>& texture, uint32_t x, uint32_t y,
		uint32_t w, uint32_t h, uint32_t mip) {
		if (!texture || m_pbo[0] == 0 || w == 0 || h == 0) return false;

		ReadbackLayout layout;
		if (!readbackLayout(texture->getFormat(), layout)) {
			VE_CORE_ERROR_PRINT("GpuReadback: no readback layout for texture format %d",
				(int)texture->getFormat());
			return false;
		}

		const size_t bytes = (size_t)w * h * (size_t)getChannelCount(texture->getFormat())
			* layout.componentBytes;
		if (bytes > m_maxBytes) {
			VE_CORE_ERROR_PRINT("GpuReadback: %zu bytes exceeds the %zu-byte buffer", bytes, m_maxBytes);
			return false;
		}

		// Least recently issued free buffer, so the two alternate instead of one
		// being overwritten while its copy is still in flight.
		int slot = -1;
		for (int i = 0; i < kSlots; ++i) {
			if (m_fence[i]) continue;
			if (slot < 0 || m_issued[i] < m_issued[slot]) slot = i;
		}
		if (slot < 0) return false;

		// Shader writes to the texture land in caches the copy would not see; this
		// makes the ones issued before this call part of what gets copied.
		glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

		// Rows are tightly packed in the request, whatever the format's channel
		// count, so the destination stride must not be rounded up.
		GLint prevAlign = 4;
		glGetIntegerv(GL_PACK_ALIGNMENT, &prevAlign);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[slot]);
		glGetTextureSubImage(texture->getRendererID(), (GLint)mip, (GLint)x, (GLint)y, 0,
			(GLsizei)w, (GLsizei)h, 1, layout.format, layout.type, (GLsizei)bytes, (void*)0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		glPixelStorei(GL_PACK_ALIGNMENT, prevAlign);

		m_fence[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		m_issued[slot] = ++m_issueCount;
		return true;
	}

	bool OpenGLGpuReadback::tryFetch(void* dst, size_t bytes) {
		if (!dst || bytes == 0 || bytes > m_maxBytes) return false;

		int slot = -1;
		for (int i = 0; i < kSlots; ++i) {
			if (!m_fence[i]) continue;
			if (slot < 0 || m_issued[i] < m_issued[slot]) slot = i;
		}
		if (slot < 0) return false;

		const GLenum status = glClientWaitSync((GLsync)m_fence[slot], GL_SYNC_FLUSH_COMMANDS_BIT, 0);
		if (status == GL_WAIT_FAILED) {
			VE_CORE_ERROR_PRINT("GpuReadback: fence wait failed");
			glDeleteSync((GLsync)m_fence[slot]);
			m_fence[slot] = nullptr;
			return false;
		}
		if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED) return false;

		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[slot]);
		void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)bytes, GL_MAP_READ_BIT);
		bool ok = false;
		if (mapped) {
			std::memcpy(dst, mapped, bytes);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			ok = true;
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		glDeleteSync((GLsync)m_fence[slot]);
		m_fence[slot] = nullptr;
		return ok;
	}

}

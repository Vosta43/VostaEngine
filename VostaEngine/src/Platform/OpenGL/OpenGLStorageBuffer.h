#pragma once

#include "Renderer/StorageBuffer.h"

namespace ve {
	
	class OpenGLStorageBuffer : public StorageBuffer {
	public:
		
		OpenGLStorageBuffer(size_t size, const void* data);

		OpenGLStorageBuffer(const OpenGLStorageBuffer&) = delete;
		OpenGLStorageBuffer& operator=(const OpenGLStorageBuffer&) = delete;
		OpenGLStorageBuffer(OpenGLStorageBuffer&& other) noexcept;
		OpenGLStorageBuffer& operator=(OpenGLStorageBuffer&& other) noexcept;

		void setData(const void* data, size_t size, size_t offset = 0) override;

		void bind(size_t point) const override;
		void unbind(size_t point) const override;

		size_t getSize() const override { return m_size; }


	private:
		uint32_t m_rendererID = 0;
		size_t m_size = 0;
		void* m_mappedPtr = nullptr;
	};

}
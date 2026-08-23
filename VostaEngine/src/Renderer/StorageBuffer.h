#pragma once

#include "Core/Core.h"

namespace ve {
	
	class StorageBuffer {
	public:
		StorageBuffer() {};
		virtual ~StorageBuffer() = default;

		virtual void bind(size_t point) const = 0;
		virtual void unbind(size_t point) const = 0;
		virtual void setData(const void* data, size_t size, size_t offset = 0) = 0;
		
		virtual size_t getSize() const = 0;

		static Ref<StorageBuffer> create(size_t size, const void* data = nullptr);

	private:

		
	};

}
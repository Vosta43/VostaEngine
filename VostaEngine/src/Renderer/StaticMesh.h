#pragma once

#include "Core/Core.h"
#include "Renderer/VertexArray.h"
#include "Asset/StaticMeshResource.h"

#include <string>

namespace ve {

	class VE_API StaticMesh {
	public:
		virtual ~StaticMesh() = default;

		static Ref<StaticMesh> create(const std::string& path);
		static Ref<StaticMesh> create(const Ref<StaticMeshResource>& staticMeshResource);

		virtual void bind() const = 0;
		virtual void unbind() const = 0;

		virtual void draw() const = 0;
		virtual void draw(uint32_t startIndex, uint32_t indexCount) const = 0;

		virtual uint32_t getVertexCount() const = 0;
		virtual uint32_t getIndexCount() const = 0;

		virtual const Ref<VertexArray>& getVertexArray() const = 0;
		virtual const std::vector<SubMeshResource>& getSubmeshes() const = 0;

		virtual glm::vec3 getAabbMin() const = 0;
		virtual glm::vec3 getAabbMax() const = 0;

	private:

	};

}

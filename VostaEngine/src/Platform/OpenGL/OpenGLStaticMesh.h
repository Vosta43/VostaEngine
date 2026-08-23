#pragma once

#include "Renderer/StaticMesh.h"
#include "Asset/StaticMeshResource.h"
#include "Renderer/Buffer.h"
#include "Renderer/VertexArray.h"

namespace ve {

	class VE_API OpenGLStaticMesh : public StaticMesh {
	public:
		OpenGLStaticMesh(Ref<StaticMeshResource> StaticMeshResource);

		void bind() const override;
		void unbind() const override;

		void draw() const override;

		uint32_t getVertexCount() const override;
		uint32_t getIndexCount() const override;
		const Ref<VertexArray>& getVertexArray() const override { return m_vertexArray; }

		void draw(uint32_t startIndex, uint32_t indexCount) const override;
		const std::vector<SubMeshResource>& getSubmeshes() const { return m_submeshes; }

		glm::vec3 getAabbMin() const override { return m_aabbMin; }
		glm::vec3 getAabbMax() const override { return m_aabbMax; }

	private:
		Ref<VertexArray> m_vertexArray;
		uint32_t m_vertexCount = 0;
		uint32_t m_indexCount = 0;

		glm::vec3 m_aabbMin{ 0.0f };
		glm::vec3 m_aabbMax{ 0.0f };

		std::vector<SubMeshResource> m_submeshes;
	};
}

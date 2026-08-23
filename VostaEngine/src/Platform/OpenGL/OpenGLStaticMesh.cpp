#include "vepch.h"
#include "OpenGLStaticMesh.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <limits>

namespace ve {

    OpenGLStaticMesh::OpenGLStaticMesh(Ref<StaticMeshResource> resource) {
        if (!resource || resource->vertexBuffer.empty()) {
            VE_CORE_ERROR_PRINT("Failed to create OpenGLStaticMesh");
            return;
        }

        m_vertexCount = static_cast<uint32_t>(resource->vertexBuffer.size());
        m_indexCount = static_cast<uint32_t>(resource->indexBuffer.size());

        // Compute AABB
        m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
        m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
        for (auto& v : resource->vertexBuffer) {
            m_aabbMin = glm::min(m_aabbMin, v.position);
            m_aabbMax = glm::max(m_aabbMax, v.position);
        }

        m_vertexArray = VertexArray::create();

        // VBO
        Ref<VertexBuffer> vbo = VertexBuffer::create(
            reinterpret_cast<float*>(resource->vertexBuffer.data()),
            static_cast<uint32_t>(resource->vertexBuffer.size() * sizeof(Vertex))
        );

        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float3, "a_Normal"   },
            { ShaderDataType::Float3, "a_Tangent"  },
        };
        vbo->setLayout(layout);
        m_vertexArray->addVertexBuffer(vbo);

        // IBOFailed to open OBJ file
        if (!resource->indexBuffer.empty()) {
            Ref<IndexBuffer> ibo = IndexBuffer::create(
                reinterpret_cast<uint32_t*>(resource->indexBuffer.data()),
                static_cast<uint32_t>(resource->indexBuffer.size())
            );
            m_vertexArray->setIndexBuffer(ibo);
        }

        VE_CORE_SUCCESS_PRINT("OpenGLStaticMesh: %u vertices, %u indices",
            m_vertexCount, m_indexCount);

        m_submeshes = resource->subMeshes;
    }


    void OpenGLStaticMesh::bind() const {
        m_vertexArray->bind();
    }

    void OpenGLStaticMesh::unbind() const {
        m_vertexArray->unbind();
    }

    void OpenGLStaticMesh::draw() const
    {
        if (m_indexCount > 0)
        {
            glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
        }
    }

    uint32_t OpenGLStaticMesh::getVertexCount() const {
        return m_vertexCount;
    }

    uint32_t OpenGLStaticMesh::getIndexCount() const {
        return m_indexCount;
    }

    void OpenGLStaticMesh::draw(uint32_t startIndex, uint32_t indexCount) const {
        if (indexCount == 0) {
            glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
        }
        else {
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT,
                (void*)(startIndex * sizeof(uint32_t)));
        }
    }

}

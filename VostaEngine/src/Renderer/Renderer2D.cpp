#include "vepch.h"
#include "Renderer2D.h"
#include "RenderCommand.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Renderer/Shader.h"
#include "Core/Application.h"
#include "Core/ResourceManager.h"

namespace ve {
    
    struct QuadVertex {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec4 color;
    };

    struct Renderer2DData {

        static const uint32_t maxQuads = 10000;
        static const uint32_t maxVertices = maxQuads * 4;
        static const uint32_t maxIndices = maxQuads * 6;

        Ref<Shader> quadShader;
        Ref<VertexBuffer> quadVertexBuffer;
        Ref<IndexBuffer> quadIndexBuffer;
        Ref<VertexArray> quadVertexArray;

        QuadVertex* quadVertexBufferBase = nullptr;
        QuadVertex* quadVertexBufferPtr = nullptr;
        uint32_t quadIndexCount = 0;

        Ref<Texture2D> currentTexture;
          
        uint32_t drawCalls = 0;
        uint32_t quadCount = 0;

        //Picking Shader
        Ref<Shader> pickingShader;
        Ref<VertexBuffer> pickingVertexBuffer;
        Ref<IndexBuffer> pickingIndexBuffer;
        Ref<VertexArray> pickingVertexArray;

        QuadVertex* pickingVertexBufferBase = nullptr;
        QuadVertex* pickingVertexBufferPtr = nullptr;
        uint32_t pickingIndexCount = 0;
    };

    static Renderer2DData s_Data;

	void Renderer2D::init(){

        s_Data.quadVertexBuffer = VertexBuffer::create(nullptr, s_Data.maxVertices * sizeof(QuadVertex));
        s_Data.quadVertexBuffer->setLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float4, "a_Color"    }
            });

        uint32_t* indices = new uint32_t[s_Data.maxIndices];
        uint32_t offset = 0;
        for (uint32_t i = 0; i < s_Data.maxIndices; i += 6) {
            indices[i + 0] = offset + 0;
            indices[i + 1] = offset + 1;
            indices[i + 2] = offset + 2;
            indices[i + 3] = offset + 2;
            indices[i + 4] = offset + 3;
            indices[i + 5] = offset + 0;
            offset += 4;
        }
        s_Data.quadIndexBuffer = IndexBuffer::create(indices, s_Data.maxIndices);
        delete[] indices;

        s_Data.quadVertexArray = VertexArray::create();
        s_Data.quadVertexArray->addVertexBuffer(s_Data.quadVertexBuffer);
        s_Data.quadVertexArray->setIndexBuffer(s_Data.quadIndexBuffer);

        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("VostaEngine/resources/shaders/Texture.glsl");
        s_Data.quadShader = shaderLib.get("Texture");

        s_Data.quadVertexBufferBase = new QuadVertex[s_Data.maxVertices];


        //Picking shader init
        s_Data.pickingVertexBuffer = VertexBuffer::create(nullptr, s_Data.maxVertices * sizeof(QuadVertex));
        
        s_Data.pickingVertexBuffer->setLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float4, "a_Color"    }
            });

        uint32_t* pickingIndices = new uint32_t[s_Data.maxIndices];
        offset = 0;
        for (uint32_t i = 0; i < s_Data.maxIndices; i += 6) {
            pickingIndices[i + 0] = offset + 0;
            pickingIndices[i + 1] = offset + 1;
            pickingIndices[i + 2] = offset + 2;
            pickingIndices[i + 3] = offset + 2;
            pickingIndices[i + 4] = offset + 3;
            pickingIndices[i + 5] = offset + 0;
            offset += 4;
        }
        s_Data.pickingIndexBuffer = IndexBuffer::create(pickingIndices, s_Data.maxIndices);
        delete[] pickingIndices;

        s_Data.pickingVertexArray = VertexArray::create();
        s_Data.pickingVertexArray->addVertexBuffer(s_Data.pickingVertexBuffer);
        s_Data.pickingVertexArray->setIndexBuffer(s_Data.pickingIndexBuffer);

        s_Data.pickingVertexBufferBase = new QuadVertex[s_Data.maxVertices];

	}
    void Renderer2D::beginScene(const glm::mat4& viewProjectionMatrix) {

        s_Data.quadShader->bind();
        s_Data.quadShader->setMat4("u_ViewProjection", viewProjectionMatrix);
        s_Data.quadShader->setMat4("u_Transform", glm::mat4(1.0f));

        s_Data.quadVertexBufferPtr = s_Data.quadVertexBufferBase;
        s_Data.quadIndexCount = 0;
        s_Data.drawCalls = 0;
        s_Data.quadCount = 0;

    }
    void Renderer2D::beginPickingScene(const glm::mat4& viewProjectionMatrix, const Ref<Shader>& pickingShader) {
        s_Data.pickingShader = pickingShader;
        s_Data.pickingShader->bind();
        s_Data.pickingShader->setMat4("u_ViewProjection", viewProjectionMatrix);

        s_Data.pickingVertexBufferPtr = s_Data.pickingVertexBufferBase;
        s_Data.pickingIndexCount = 0;
    }

    void Renderer2D::endPickingScene() {
        if (s_Data.pickingIndexCount == 0) return;

        uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.pickingVertexBufferPtr - (uint8_t*)s_Data.pickingVertexBufferBase);
        s_Data.pickingVertexBuffer->setData(s_Data.pickingVertexBufferBase, dataSize);

        RenderCommand::drawIndexed(s_Data.pickingVertexArray);

        s_Data.pickingVertexBufferPtr = s_Data.pickingVertexBufferBase;
        s_Data.pickingIndexCount = 0;
    }

    void Renderer2D::endScene() {
        flush();
    }

    void Renderer2D::flush() {
        if (s_Data.quadIndexCount == 0) return;

        // Calculate how many bytes have been written into the CPU-side ring buffer.
        // The buffer range [quadVertexBufferBase, quadVertexBufferPtr) contains the
        // per-quad vertex data (position, UV, color) accumulated since the last flush.
        uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.quadVertexBufferPtr - (uint8_t*)s_Data.quadVertexBufferBase);
        s_Data.quadVertexBuffer->setData(s_Data.quadVertexBufferBase, dataSize);

        s_Data.quadShader->setInt("u_Texture", 0);

        RenderCommand::drawIndexed(s_Data.quadVertexArray);
        s_Data.drawCalls++;

        s_Data.quadVertexBufferPtr = s_Data.quadVertexBufferBase;
        s_Data.quadIndexCount = 0;

        s_Data.currentTexture.reset();
    }
    void Renderer2D::drawQuad(const glm::mat4& transform, const glm::vec2& size, AssetHandle textureHandle, const glm::vec4& tint) {

        Ref<Texture2D> texture = ResourceManager::get<Texture2D>(textureHandle);
        if (!texture) return;

        if (texture != s_Data.currentTexture) {
            flush();
            s_Data.currentTexture = texture;
            texture->bind(0);
        }

        float halfW = size.x * 0.5f;
        float halfH = size.y * 0.5f;

        glm::vec4 localCorners[4] = {
            { -halfW, -halfH, 0.0f, 1.0f },
            {  halfW, -halfH, 0.0f, 1.0f },
            {  halfW,  halfH, 0.0f, 1.0f },
            { -halfW,  halfH, 0.0f, 1.0f },
        };

        glm::vec3 worldCorners[4];
        for (int i = 0; i < 4; ++i) {
            glm::vec4 world = transform * localCorners[i];
            worldCorners[i] = glm::vec3(world) / world.w;
        }

        glm::vec2 texCoords[4] = {
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 1.0f, 1.0f },
            { 0.0f, 1.0f },
        };

        for (int i = 0; i < 4; ++i) {
            s_Data.quadVertexBufferPtr->position = worldCorners[i];
            s_Data.quadVertexBufferPtr->texCoord = texCoords[i];
            s_Data.quadVertexBufferPtr->color = tint;
            s_Data.quadVertexBufferPtr++;
        }

        s_Data.quadIndexCount += 6;
        s_Data.quadCount++;
    }
    void Renderer2D::drawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, const glm::vec4& color) {
        const auto& texCoords = subTexture->getTexCoord();
        const auto& texture = subTexture->getTexture();

        if (texture != s_Data.currentTexture) {
            
            flush();
            s_Data.currentTexture = texture;
            texture->bind(0);
        }

        s_Data.quadVertexBufferPtr->position = { position.x, position.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = texCoords[0];
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadVertexBufferPtr->position = { position.x + size.x, position.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = texCoords[1];
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadVertexBufferPtr->position = { position.x + size.x, position.y + size.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = texCoords[2];
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadVertexBufferPtr->position = { position.x, position.y + size.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = texCoords[3];
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadIndexCount += 6;
        s_Data.quadCount++;
    }
    void Renderer2D::drawQuad(const glm::vec3& position, const glm::vec2& size, AssetHandle textureHandle, const glm::vec4& color) {
        
        Ref<Texture2D> texture = ResourceManager::get<Texture2D>(textureHandle);
        if (texture != s_Data.currentTexture) {
            flush();
            s_Data.currentTexture = texture;
            texture->bind(0);
        }

        s_Data.quadVertexBufferPtr->position = { position.x, position.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = { 0.0f, 0.0f };
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadVertexBufferPtr->position = { position.x + size.x, position.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = { 1.0f, 0.0f };
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadVertexBufferPtr->position = { position.x + size.x, position.y + size.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = { 1.0f, 1.0f };
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadVertexBufferPtr->position = { position.x, position.y + size.y, position.z };
        s_Data.quadVertexBufferPtr->texCoord = { 0.0f, 1.0f };
        s_Data.quadVertexBufferPtr->color = color;
        s_Data.quadVertexBufferPtr++;

        s_Data.quadIndexCount += 6;
        s_Data.quadCount++;
    }
    void Renderer2D::drawPickingQuad(const glm::vec3& position, const glm::vec2& size, uint32_t entityID) {
        float idAsFloat;
        memcpy(&idAsFloat, &entityID, sizeof(float));
        glm::vec4 idColor = { idAsFloat, 0.0f, 0.0f, 1.0f };

        s_Data.pickingVertexBufferPtr->position = { position.x, position.y, position.z };
        s_Data.pickingVertexBufferPtr->texCoord = { 0.0f, 0.0f };
        s_Data.pickingVertexBufferPtr->color = idColor;
        s_Data.pickingVertexBufferPtr++;

        s_Data.pickingVertexBufferPtr->position = { position.x + size.x, position.y, position.z };
        s_Data.pickingVertexBufferPtr->texCoord = { 1.0f, 0.0f };
        s_Data.pickingVertexBufferPtr->color = idColor;
        s_Data.pickingVertexBufferPtr++;

        s_Data.pickingVertexBufferPtr->position = { position.x + size.x, position.y + size.y, position.z };
        s_Data.pickingVertexBufferPtr->texCoord = { 1.0f, 1.0f };
        s_Data.pickingVertexBufferPtr->color = idColor;
        s_Data.pickingVertexBufferPtr++;

        s_Data.pickingVertexBufferPtr->position = { position.x, position.y + size.y, position.z };
        s_Data.pickingVertexBufferPtr->texCoord = { 0.0f, 1.0f };
        s_Data.pickingVertexBufferPtr->color = idColor;
        s_Data.pickingVertexBufferPtr++;

        s_Data.pickingIndexCount += 6;
    }

    void Renderer2D::setBlending(bool enabled) {
        RenderCommand::setBlend(enabled);
        if (enabled) {
            RenderCommand::setBlendFunc(
                RendererAPI::BlendFunc::SrcAlpha,
                RendererAPI::BlendFunc::OneMinusSrcAlpha
            );
        }
    }

    void Renderer2D::drawPickingQuad(const glm::mat4& transform, const glm::vec2& size, uint32_t entityID) {
        float idAsFloat;
        memcpy(&idAsFloat, &entityID, sizeof(float));
        glm::vec4 idColor = { idAsFloat, 0.0f, 0.0f, 1.0f };

        float halfW = size.x * 0.5f;
        float halfH = size.y * 0.5f;

        glm::vec4 localCorners[4] = {
            { -halfW, -halfH, 0.0f, 1.0f },
            {  halfW, -halfH, 0.0f, 1.0f },
            {  halfW,  halfH, 0.0f, 1.0f },
            { -halfW,  halfH, 0.0f, 1.0f },
        };

        glm::vec2 texCoords[4] = {
            { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
        };

        for (int i = 0; i < 4; ++i) {
            glm::vec4 world = transform * localCorners[i];
            s_Data.pickingVertexBufferPtr->position = glm::vec3(world) / world.w;
            s_Data.pickingVertexBufferPtr->texCoord = texCoords[i];
            s_Data.pickingVertexBufferPtr->color = idColor;
            s_Data.pickingVertexBufferPtr++;
        }

        s_Data.pickingIndexCount += 6;
    }


}
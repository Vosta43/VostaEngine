#pragma once
#include "Renderer/Shader.h"
#include "Core/Core.h"
#include "Renderer/Light.h"
#include "Renderer/StorageBuffer.h"

namespace ve {
	class VE_API OpenGLShader : public Shader {
	public:
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		OpenGLShader(const std::string& filePath);
		~OpenGLShader();

		void bind() const override;
		void unbind() const override;

		std::string getName() const override;
		
		void setFloat(const std::string& name, float value) override;
		void setFloat2(const std::string& name, const glm::vec2& value) override;
		void setFloat3(const std::string& name, const glm::vec3& value) override;
		void setFloat4(const std::string& name, const glm::vec4& value) override;
		void setMat4(const std::string& name, const glm::mat4& value) override;
		void setInt(const std::string& name, int value) override;
		void setTexture(const std::string& name, const Ref<Texture2D>& texture, uint32_t slot) override;
		void setTextureCube(const std::string& name, const Ref<TextureCubeMap>& texture, uint32_t slot) override;
		void setTexture3D(const std::string& name, const Ref<Texture3D>& texture, uint32_t slot) override;

		void uploadUniformFloat(const std::string& name, float values);
		void uploadUniformFloat2(const std::string& name, const glm::vec2& values);
		void uploadUniformFloat3(const std::string& name, const glm::vec3& values);
		void uploadUniformFloat4(const std::string& name, const glm::vec4& values);
		void uploadUniformMat4(const std::string& name, const glm::mat4& matrix);
		void uploadUniformInt(const std::string& name, int value);

		void setLightSSBO(const std::vector<GpuLightData>& lights) override;
		void bindLightSSBO(size_t bindingPoint = 1) override;
		void unbindLightSSBO(size_t bindingPoint = 1) override;
		
	private:
		void compileFromSources(const std::string& vertexSrc, const std::string& fragmentSrc);
		
		uint32_t shaderId;
		std::string m_name;

		Ref<StorageBuffer> m_lightSSBO;
	};


}
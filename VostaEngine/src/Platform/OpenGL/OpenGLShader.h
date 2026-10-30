#pragma once
#include "Renderer/Shader.h"
#include "Core/Core.h"
#include "Renderer/GpuLightData.h"
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

		// False when compileFromSources bailed (compile/link failure or a bad
		// file/include), leaving shaderId at 0. See Shader::isValid.
		bool isValid() const override;

		int getUniformLocation(const std::string& name) override;

		void setFloat(int location, float value) override;
		void setFloat2(int location, const glm::vec2& value) override;
		void setFloat3(int location, const glm::vec3& value) override;
		void setFloat4(int location, const glm::vec4& value) override;
		void setMat4(int location, const glm::mat4& value) override;
		void setInt(int location, int value) override;

		void setFloat(const std::string& name, float value) override;
		void setFloat2(const std::string& name, const glm::vec2& value) override;
		void setFloat3(const std::string& name, const glm::vec3& value) override;
		void setFloat4(const std::string& name, const glm::vec4& value) override;
		void setMat4(const std::string& name, const glm::mat4& value) override;
		void setInt(const std::string& name, int value) override;
		void setTexture(const std::string& name, const Ref<Texture2D>& texture, uint32_t slot) override;
		void setTextureArray(const std::string& name, const Ref<Texture2DArray>& texture, uint32_t slot) override;
		void setTextureCube(const std::string& name, const Ref<TextureCubeMap>& texture, uint32_t slot) override;
		void setTexture3D(const std::string& name, const Ref<Texture3D>& texture, uint32_t slot) override;
		void setImage(const std::string& name, const Ref<Texture>& texture, uint32_t unit, ImageAccess access) override;
		void bindUniformBlock(const std::string& blockName, uint32_t bindingPoint) override;

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

		uint32_t shaderId = 0;
		std::string m_name;

		// Block indices and uniform locations are fixed once the program links, so
		// both tables are filled there; uploading a uniform is a map lookup, not a
		// driver query.
		std::unordered_map<std::string, uint32_t> m_uniformBlockIndex;
		std::unordered_map<std::string, int> m_uniformLocations;

		void cacheUniformLocations();
		// Location for `name`, or -1. A name missing from the link-time table is
		// queried once and remembered, so a miss also costs one query in total.
		int  uniformLocation(const std::string& name);

		// Light storage, grown to the live light count. Uploaded with glBufferData
		// rather than a persistently mapped StorageBuffer: the budget is
		// unbounded (one element per light in the scene), and a plain re-upload
		// has no host/device visibility or write-while-reading hazard to get
		// wrong.
		uint32_t m_lightSSBOId = 0;
		size_t   m_lightSSBOBytes = 0;
	};


}
#pragma once

#include <string>
#include <unordered_map>
#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/Light.h"

#include <glm.hpp>

namespace ve {
	class VE_API Shader {
	public:
		virtual ~Shader();

		virtual void bind() const = 0;
		virtual void unbind() const = 0;
		
		virtual void setFloat(const std::string& name, float value) {};
		virtual void setFloat2(const std::string& name,const glm::vec2& value) {};
		virtual void setFloat3(const std::string& name, const glm::vec3& value) {};
		virtual void setFloat4(const std::string& name, const glm::vec4& value) {};
		virtual void setMat4(const std::string& name, const glm::mat4& value) {};
		virtual void setInt(const std::string& name, int value) {};
		virtual void setTexture(const std::string& name, const Ref<Texture2D>& texture, uint32_t slot) {};
		virtual void setTextureCube(const std::string& name, const Ref<TextureCubeMap>& texture, uint32_t slot) {};
		virtual void setTexture3D(const std::string& name, const Ref<Texture3D>& texture, uint32_t slot) {};
		static Ref<Shader> create(const std::string& name,const std::string& vertexSrc, const std::string& fragmentSrc);
		static Ref<Shader> create(const std::string& filePath);

		virtual void setLightSSBO(const std::vector<GpuLightData>& lights) {};
		virtual void bindLightSSBO(size_t bindingPoint = 1) {};
		virtual void unbindLightSSBO(size_t bindingPoint = 1) {};

		virtual std::string getName() const = 0;
	private:

	};

	class VE_API ShaderLibrary {
	public:
		void add(const Ref<Shader>& shader);
		Ref<Shader>& get(const std::string& name);

		void load(const std::string& filePath);
		void load(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
	private:
		std::unordered_map<std::string,Ref<Shader>> m_shaders;
	};


}
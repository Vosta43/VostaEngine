#include "vepch.h"
#include "Shader.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>

#include "Platform/OpenGL/OpenGLShader.h"
#include <filesystem>

namespace ve {


    Ref<Shader> Shader::create(const std::string& name,const std::string& vertexSrc, const std::string& fragmentSrc){
        //TODO:switch the graphicAPI.But it is not an important job yet
        return std::make_shared<OpenGLShader>(name,vertexSrc,fragmentSrc);

    }

    Ref<Shader> Shader::create(const std::string& filePath){
        //TODO:switch the graphicAPI.But it is not an important job yet
        std::string resolvedPath = toAbsolute(filePath);
        return std::make_shared<OpenGLShader>(resolvedPath);
    }

    void ShaderLibrary::add(const Ref<Shader>& shader){

        const auto& name = shader.get()->getName();
        m_shaders[name] = shader;
    }

    Ref<Shader>& ShaderLibrary::get(const std::string& name){

        return m_shaders[name];
    }

    void ShaderLibrary::load(const std::string& filePath){
        // Already loaded -- skip to avoid replacing a working shader with a
        // broken one when a throwaway RenderPipeline re-initializes passes.
        std::string name = std::filesystem::path(filePath).stem().string();
        if (m_shaders.find(name) != m_shaders.end()) {
            return;
        }
        auto shader = Shader::create(filePath);
        add(shader);
    }

    void ShaderLibrary::load(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc){
        auto shader = Shader::create(name,vertexSrc,fragmentSrc);
        add(shader);
    }

}
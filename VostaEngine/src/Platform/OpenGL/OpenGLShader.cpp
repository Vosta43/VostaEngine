#include "vepch.h"
#include "OpenGLShader.h"
#include "Core/Log.h"
#include <glad/glad.h>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include <gtc/type_ptr.hpp>

namespace {

    // Recursively inlines `#include "file"` directives. Relative paths resolve
    // against the directory of the including file. Each file is inlined at most
    // once (include-once) to avoid duplicate definitions and include cycles.
    std::string resolveIncludes(const std::string& source,
                                const std::filesystem::path& dir,
                                std::unordered_set<std::string>& included,
                                std::string& error) {
        std::string result;
        result.reserve(source.size());

        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            std::string trimmed = line;
            size_t first = trimmed.find_first_not_of(" \t");
            trimmed = (first == std::string::npos) ? std::string() : trimmed.substr(first);

            if (trimmed.rfind("#include", 0) == 0) {
                size_t openQuote  = trimmed.find('"');
                size_t closeQuote = (openQuote == std::string::npos) ? std::string::npos
                                                                     : trimmed.find('"', openQuote + 1);
                if (openQuote == std::string::npos || closeQuote == std::string::npos) {
                    error = "Malformed #include directive: " + line;
                    return "";
                }

                std::string includeName = trimmed.substr(openQuote + 1, closeQuote - openQuote - 1);
                std::filesystem::path fullPath = dir / includeName;

                std::error_code ec;
                std::filesystem::path canonical = std::filesystem::weakly_canonical(fullPath, ec);
                std::string key = ec ? fullPath.string() : canonical.string();

                if (included.count(key))
                    continue;  // already inlined

                std::ifstream incFile(fullPath);
                if (!incFile.is_open()) {
                    error = "Failed to open included shader file: " + fullPath.string();
                    return "";
                }

                included.insert(key);

                std::stringstream incBuffer;
                incBuffer << incFile.rdbuf();

                std::string incResult = resolveIncludes(incBuffer.str(), fullPath.parent_path(), included, error);
                if (!error.empty())
                    return "";

                result += incResult;
                result += "\n";
            } else {
                result += line;
                result += "\n";
            }
        }
        return result;
    }

} // anonymous namespace

namespace ve {

	OpenGLShader::OpenGLShader(const std::string & name,const std::string& vertexSrc, const std::string& fragmentSrc)
        :m_name(name){

        compileFromSources(vertexSrc,fragmentSrc);
	}

    OpenGLShader::OpenGLShader(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            VE_CORE_ERROR_PRINT("Failed to open shader file: %s", filePath.c_str());
            shaderId = 0;
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        // Inline #include directives before splitting into stages.
        std::unordered_set<std::string> included;
        std::string includeError;
        std::filesystem::path sourceDir = std::filesystem::path(filePath).parent_path();
        source = resolveIncludes(source, sourceDir, included, includeError);
        if (!includeError.empty()) {
            VE_CORE_ERROR_PRINT("%s", includeError.c_str());
            shaderId = 0;
            return;
        }

        // Split the source by the #shader vertex / #shader fragment directives.
        auto vertexPos = source.find("#shader vertex");
        auto fragmentPos = source.find("#shader fragment");

        if (vertexPos == std::string::npos || fragmentPos == std::string::npos) {
            VE_CORE_ERROR_PRINT("Shader file missing #shader vertex or #shader fragment directive: %s", filePath.c_str());
            shaderId = 0;
            return;
        }

        std::string vertexSrc = source.substr(vertexPos + 15, fragmentPos - vertexPos - 15);
        std::string fragmentSrc = source.substr(fragmentPos + 17);

        compileFromSources(vertexSrc, fragmentSrc);

        //Extract shader name from filename
        std::string path = filePath;
        size_t lastSlash = path.find_last_of("/\\");
        std::string fileName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

        size_t dotPos = fileName.find_last_of('.');
        m_name = (dotPos == std::string::npos) ? fileName : fileName.substr(0, dotPos);
    }

    void OpenGLShader::bind() const {
        glUseProgram(shaderId);
    }

    void OpenGLShader::unbind() const {
        glUseProgram(0);
    }

    std::string OpenGLShader::getName() const {
        return m_name;
    }

    void OpenGLShader::setFloat(const std::string& name, float value)
    {
        uploadUniformFloat(name, value);
    }

    void OpenGLShader::setFloat2(const std::string& name, const glm::vec2& value)
    {
        uploadUniformFloat2(name, value);
    }

    void OpenGLShader::setFloat3(const std::string& name, const glm::vec3& value)
    {
        uploadUniformFloat3(name,value);
    }

    void OpenGLShader::setFloat4(const std::string& name, const glm::vec4& value)
    {
        uploadUniformFloat4(name,value);
    }

    void OpenGLShader::setMat4(const std::string& name, const glm::mat4& value)
    {
        uploadUniformMat4(name,value);
    }

    void OpenGLShader::setInt(const std::string& name, int value)
    {
        uploadUniformInt(name,value);
    }

    void OpenGLShader::setTexture(const std::string& name, const Ref<Texture2D>& texture, uint32_t slot)
    {
        if (!texture) return;
        texture->bind(slot);
        setInt(name, slot);
    }

    void OpenGLShader::setTextureCube(const std::string& name, const Ref<TextureCubeMap>& texture, uint32_t slot)
    {
        if(!texture) return;
        texture->bind(slot);
        setInt(name,slot);
    }

    void OpenGLShader::setTexture3D(const std::string& name, const Ref<Texture3D>& texture, uint32_t slot)
    {
        if (!texture) return;
        texture->bind(slot);
        setInt(name, slot);
    }

    void OpenGLShader::uploadUniformFloat(const std::string& name, float values)
    {
        GLint location = glGetUniformLocation(shaderId, name.c_str());
        glUniform1f(location, values);
    }

    void OpenGLShader::uploadUniformFloat2(const std::string& name, const glm::vec2& values)
    {
        GLint location = glGetUniformLocation(shaderId, name.c_str());
        glUniform2f(location, values.x,values.y);
    }

    void OpenGLShader::uploadUniformFloat3(const std::string& name, const glm::vec3& values)
    {
        GLint location = glGetUniformLocation(shaderId, name.c_str());
        glUniform3f(location, values.x, values.y,values.z);
    }

    void OpenGLShader::uploadUniformFloat4(const std::string& name, const glm::vec4& values){
        
        GLuint location = glGetUniformLocation(shaderId, name.c_str());
        glUniform4f(location, values.x, values.y, values.z, values.w);
    }

    void OpenGLShader::uploadUniformMat4(const std::string& name, const glm::mat4& matrix){

        GLuint location = glGetUniformLocation(shaderId, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    void OpenGLShader::uploadUniformInt(const std::string& name, int value){

        GLuint location = glGetUniformLocation(shaderId, name.c_str());
        glUniform1i(location, value);
    }

    void OpenGLShader::setLightSSBO(const std::vector<GpuLightData>& lights) {
        if (lights.empty()) return;

        size_t neededSize = lights.size() * sizeof(GpuLightData);

        if (!m_lightSSBO || m_lightSSBO->getSize() < neededSize) {
            m_lightSSBO = StorageBuffer::create(neededSize, lights.data());
        }
        else {
            m_lightSSBO->setData(lights.data(), neededSize);
        }

        setInt("u_LightCount", static_cast<int>(lights.size()));
    }

    void OpenGLShader::bindLightSSBO(size_t bindingPoint) {
        if (m_lightSSBO) {
            m_lightSSBO->bind(bindingPoint);
        }
    }

    void OpenGLShader::unbindLightSSBO(size_t bindingPoint) {
        if (m_lightSSBO) {
            m_lightSSBO->unbind(bindingPoint);
        }
    }

    void OpenGLShader::compileFromSources(const std::string& vertexSrc, const std::string& fragmentSrc){
        
        auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const GLchar* source = vertexSrc.c_str();
        glShaderSource(vertexShader, 1, &source, nullptr);
        glCompileShader(vertexShader);

        GLint isCompiled = 0;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE) {
            GLint maxLength = 0;
            glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);
            std::string infoLog(maxLength, '\0');
            glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &infoLog[0]);
            glDeleteShader(vertexShader);
            VE_CORE_ERROR_PRINT("Vertex shader compilation failed: %s", infoLog.c_str());
            shaderId = 0;
            return;
        }

        auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        source = fragmentSrc.c_str();
        glShaderSource(fragmentShader, 1, &source, nullptr);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE) {
            GLint maxLength = 0;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);
            std::string infoLog(maxLength, '\0');
            glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &infoLog[0]);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            VE_CORE_ERROR_PRINT("Fragment shader compilation failed: %s", infoLog.c_str());
            shaderId = 0;
            return;
        }

        shaderId = glCreateProgram();
        glAttachShader(shaderId, vertexShader);
        glAttachShader(shaderId, fragmentShader);
        glLinkProgram(shaderId);

        GLint isLinked = 0;
        glGetProgramiv(shaderId, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE) {
            GLint maxLength = 0;
            glGetProgramiv(shaderId, GL_INFO_LOG_LENGTH, &maxLength);
            std::string infoLog(maxLength, '\0');
            glGetProgramInfoLog(shaderId, maxLength, &maxLength, &infoLog[0]);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(shaderId);
            VE_CORE_ERROR_PRINT("Shader program linking failed: %s", infoLog.c_str());
            shaderId = 0;
            return;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

    }

    OpenGLShader::~OpenGLShader() {
        if (shaderId) {
            glDeleteProgram(shaderId);
        }
    }

    Shader::~Shader() = default;


}
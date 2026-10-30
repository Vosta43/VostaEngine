#include "vepch.h"
#include "OpenGLShader.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Renderer/Texture.h"
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

    bool OpenGLShader::isValid() const {
        return shaderId != 0;
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

    void OpenGLShader::setTextureArray(const std::string& name, const Ref<Texture2DArray>& texture, uint32_t slot)
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

    void OpenGLShader::setImage(const std::string& name, const Ref<Texture>& texture, uint32_t unit, ImageAccess access)
    {
        if (!texture) return;
        texture->bindImage(unit, access);
        setInt(name, unit);
    }

    void OpenGLShader::bindUniformBlock(const std::string& blockName, uint32_t bindingPoint)
    {
        if (shaderId == 0) return;

        auto it = m_uniformBlockIndex.find(blockName);
        if (it == m_uniformBlockIndex.end()) {
            GLuint blockIndex = glGetUniformBlockIndex(shaderId, blockName.c_str());
            it = m_uniformBlockIndex.emplace(blockName, blockIndex).first;
        }
        // GL_INVALID_INDEX: the block was never declared or the linker dropped it.
        if (it->second == GL_INVALID_INDEX) return;

        glUniformBlockBinding(shaderId, it->second, bindingPoint);
    }

    void OpenGLShader::uploadUniformFloat(const std::string& name, float values)
    {
        const GLint location = uniformLocation(name);
        if (location < 0) return;
        glUniform1f(location, values);
    }

    void OpenGLShader::uploadUniformFloat2(const std::string& name, const glm::vec2& values)
    {
        const GLint location = uniformLocation(name);
        if (location < 0) return;
        glUniform2f(location, values.x,values.y);
    }

    void OpenGLShader::uploadUniformFloat3(const std::string& name, const glm::vec3& values)
    {
        const GLint location = uniformLocation(name);
        if (location < 0) return;
        glUniform3f(location, values.x, values.y,values.z);
    }

    void OpenGLShader::uploadUniformFloat4(const std::string& name, const glm::vec4& values){

        const GLint location = uniformLocation(name);
        if (location < 0) return;
        glUniform4f(location, values.x, values.y, values.z, values.w);
    }

    void OpenGLShader::uploadUniformMat4(const std::string& name, const glm::mat4& matrix){

        const GLint location = uniformLocation(name);
        if (location < 0) return;
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    void OpenGLShader::uploadUniformInt(const std::string& name, int value){

        const GLint location = uniformLocation(name);
        if (location < 0) return;
        glUniform1i(location, value);
    }

    int OpenGLShader::getUniformLocation(const std::string& name) {
        return uniformLocation(name);
    }

    void OpenGLShader::setFloat(int location, float value) {
        if (location < 0) return;
        glUniform1f(location, value);
    }

    void OpenGLShader::setFloat2(int location, const glm::vec2& value) {
        if (location < 0) return;
        glUniform2f(location, value.x, value.y);
    }

    void OpenGLShader::setFloat3(int location, const glm::vec3& value) {
        if (location < 0) return;
        glUniform3f(location, value.x, value.y, value.z);
    }

    void OpenGLShader::setFloat4(int location, const glm::vec4& value) {
        if (location < 0) return;
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }

    void OpenGLShader::setMat4(int location, const glm::mat4& value) {
        if (location < 0) return;
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }

    void OpenGLShader::setInt(int location, int value) {
        if (location < 0) return;
        glUniform1i(location, value);
    }

    void OpenGLShader::cacheUniformLocations()
    {
        m_uniformLocations.clear();
        m_uniformBlockIndex.clear();
        if (shaderId == 0) return;

        GLint activeBlockCount = 0;
        glGetProgramiv(shaderId, GL_ACTIVE_UNIFORM_BLOCKS, &activeBlockCount);
        for (GLint i = 0; i < activeBlockCount; ++i) {
            GLint nameLength = 0;
            glGetActiveUniformBlockiv(shaderId, static_cast<GLuint>(i), GL_UNIFORM_BLOCK_NAME_LENGTH, &nameLength);
            if (nameLength <= 0) continue;
            std::string blockName(static_cast<size_t>(nameLength), '\0');
            GLsizei written = 0;
            glGetActiveUniformBlockName(shaderId, static_cast<GLuint>(i), nameLength, &written, blockName.data());
            if (written <= 0) continue;
            // The reported length includes the terminating NUL.
            blockName.resize(static_cast<size_t>(written - 1));
            m_uniformBlockIndex.emplace(blockName, static_cast<uint32_t>(i));
        }

        GLint activeCount = 0;
        glGetProgramiv(shaderId, GL_ACTIVE_UNIFORMS, &activeCount);
        if (activeCount <= 0) return;

        GLint maxNameLength = 0;
        glGetProgramiv(shaderId, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
        std::string name(static_cast<size_t>(maxNameLength > 0 ? maxNameLength : 1), '\0');

        for (GLint i = 0; i < activeCount; ++i) {
            GLsizei nameLength = 0;
            GLint arraySize = 0;
            GLenum type = 0;
            glGetActiveUniform(shaderId, static_cast<GLuint>(i), static_cast<GLsizei>(name.size()),
                               &nameLength, &arraySize, &type, name.data());
            if (nameLength <= 0) continue;

            std::string baseName(name.data(), static_cast<size_t>(nameLength));
            const GLint location = glGetUniformLocation(shaderId, baseName.c_str());
            if (location < 0) continue;
            m_uniformLocations.emplace(baseName, static_cast<int>(location));

            // Arrays report "foo[0]"; the element-0 location is also valid for the
            // bare name, which is how most callers spell it.
            if (arraySize > 1) {
                const size_t bracket = baseName.find('[');
                if (bracket != std::string::npos)
                    m_uniformLocations.emplace(baseName.substr(0, bracket), static_cast<int>(location));
            }
        }
    }

    int OpenGLShader::uniformLocation(const std::string& name)
    {
        auto it = m_uniformLocations.find(name);
        if (it != m_uniformLocations.end()) return it->second;

        // Misses are remembered too: a name the linker dropped should cost one
        // query over the shader's lifetime, not one per frame.
        const GLint location = shaderId ? glGetUniformLocation(shaderId, name.c_str()) : -1;
        m_uniformLocations.emplace(name, static_cast<int>(location));
        return static_cast<int>(location);
    }

    void OpenGLShader::setLightSSBO(const std::vector<GpuLightData>& lights) {
        if (lights.empty()) return;

        const GLsizeiptr size = (GLsizeiptr)(lights.size() * sizeof(GpuLightData));

        if (m_lightSSBOId == 0) {
            glGenBuffers(1, &m_lightSSBOId);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBOId);
        // glBufferData re-specifies the store: the old contents are orphaned and
        // a new block is handed to this upload, so the driver never has to
        // serialize against a draw still reading the previous frame's lights.
        // It also grows the buffer to any light count.
        glBufferData(GL_SHADER_STORAGE_BUFFER, size, lights.data(), GL_DYNAMIC_DRAW);
        m_lightSSBOBytes = (size_t)size;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        setInt("u_LightCount", static_cast<int>(lights.size()));
    }

    void OpenGLShader::bindLightSSBO(size_t bindingPoint) {
        if (m_lightSSBOId) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(bindingPoint), m_lightSSBOId);
        }
    }

    void OpenGLShader::unbindLightSSBO(size_t bindingPoint) {
        if (m_lightSSBOId) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(bindingPoint), 0);
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

        cacheUniformLocations();
    }

    OpenGLShader::~OpenGLShader() {
        if (shaderId) {
            glDeleteProgram(shaderId);
        }
        if (m_lightSSBOId) {
            glDeleteBuffers(1, &m_lightSSBOId);
            m_lightSSBOId = 0;
        }
    }

    Shader::~Shader() = default;


}
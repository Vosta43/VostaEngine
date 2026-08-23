#include "vepch.h"
#include "OpenGLStorageBuffer.h"
#include "Core/Log.h"

#include <glad/glad.h>

namespace ve {
	
    OpenGLStorageBuffer::OpenGLStorageBuffer(size_t size, const void* data)
        : m_size(size) {

        GLuint id;
        glGenBuffers(1, &id);
        m_rendererID = static_cast<uint32_t>(id);
    
       glBindBuffer(GL_SHADER_STORAGE_BUFFER,m_rendererID);
       glBufferStorage(GL_SHADER_STORAGE_BUFFER,size,data,GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
       m_mappedPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

       glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    }


    void OpenGLStorageBuffer::setData(const void* data, size_t size, size_t offset){

        if (offset + size > m_size) {
            VE_CORE_ERROR_PRINT("OpenGLStorageBuffer out of bounds");

            return;
        }
        std::memcpy(static_cast<char*>(m_mappedPtr) + offset,data,size);

    }

    void OpenGLStorageBuffer::bind(size_t point) const{

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,static_cast<GLuint>(point),m_rendererID);
    }

    void OpenGLStorageBuffer::unbind(size_t point) const{

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(point), m_rendererID);
    }

}


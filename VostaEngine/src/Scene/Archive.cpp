#include "vepch.h"
#include "Archive.h"
#include "Math/Math.h"

#include <algorithm>

namespace ve {
    BinaryArchive::BinaryArchive(const std::string& filename, ArchiveMode mode)
        : m_mode(mode), m_isGood(true), m_bufferPos(0)
    {
        std::ios::openmode flags = 0;
        if (mode == ArchiveMode::read) {
            flags = std::ios::in | std::ios::binary;
        }
        else {
            flags = std::ios::out | std::ios::binary | std::ios::trunc;
        }

        m_file.open(filename, flags);
        if (!m_file.is_open()) {
            m_isGood = false;
        }
    }

    BinaryArchive::~BinaryArchive()
    {
        if (m_mode == ArchiveMode::write && !m_buffer.empty()) {
            flush();
        }
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    template<typename T>
    void BinaryArchive::write(T val)
    {
        size_t needed = m_bufferPos + sizeof(T);
        if (needed > m_buffer.size()) {
            m_buffer.resize(max(needed, m_buffer.size() * 2));
        }
        memcpy(m_buffer.data() + m_bufferPos, &val, sizeof(T));
        m_bufferPos += sizeof(T);
    }

    template<typename T>
    void BinaryArchive::read(T& val)
    {
        if (m_bufferPos + sizeof(T) > m_buffer.size()) {
            m_isGood = false;
            return;
        }
        memcpy(&val, m_buffer.data() + m_bufferPos, sizeof(T));
        m_bufferPos += sizeof(T);
    }

    void BinaryArchive::writeString(const std::string& str)
    {
        uint32_t len = static_cast<uint32_t>(str.size());
        write(len);
        size_t needed = m_bufferPos + len;
        if (needed > m_buffer.size()) {
            m_buffer.resize(max(needed, m_buffer.size() * 2));
        }
        memcpy(m_buffer.data() + m_bufferPos, str.data(), len);
        m_bufferPos += len;
    }

    void BinaryArchive::readString(std::string& str)
    {
        uint32_t len = 0;
        read(len);
        if (!m_isGood) return;

        if (m_bufferPos + len > m_buffer.size()) {
            m_isGood = false;
            return;
        }

        str.assign(reinterpret_cast<char*>(m_buffer.data() + m_bufferPos), len);
        m_bufferPos += len;
    }

    Archive& BinaryArchive::operator<<(int32_t val)
    {
        write(val);
        return *this;
    }

    Archive& BinaryArchive::operator>>(int32_t& val)
    {
        read(val);
        return *this;
    }

    Archive& BinaryArchive::operator<<(float val)
    {
        write(val);
        return *this;
    }

    Archive& BinaryArchive::operator>>(float& val)
    {
        read(val);
        return *this;
    }

    Archive& BinaryArchive::operator<<(const std::string& val)
    {
        writeString(val);
        return *this;
    }

    Archive& BinaryArchive::operator>>(std::string& val)
    {
        readString(val);
        return *this;
    }

    void BinaryArchive::writeBytes(const void* data, size_t size)
    {
        size_t needed = m_bufferPos + size;
        if (needed > m_buffer.size()) {
            m_buffer.resize(max(needed, m_buffer.size() * 2));
        }
        memcpy(m_buffer.data() + m_bufferPos, data, size);
        m_bufferPos += size;
    }

    void BinaryArchive::readBytes(void* data, size_t size)
    {
        if (m_bufferPos + size > m_buffer.size()) {
            m_isGood = false;
            return;
        }
        memcpy(data, m_buffer.data() + m_bufferPos, size);
        m_bufferPos += size;
    }

    void BinaryArchive::flush()
    {
        if (m_mode == ArchiveMode::write && !m_buffer.empty()) {
            m_file.write(reinterpret_cast<char*>(m_buffer.data()), m_buffer.size());
            m_file.flush();
            m_buffer.clear();
            m_bufferPos = 0;
        }
    }

   



}
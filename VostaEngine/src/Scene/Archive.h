#pragma once

#include <functional>
#include <fstream>
#include <filesystem>
#include <sstream>

#include <glm.hpp>

#include "Core/Core.h"
#include "Core/Vfs.h"

namespace ve {
    class VE_API Archive {
    public:
        virtual ~Archive() = default;

        virtual Archive& operator<<(int32_t val) = 0;
        virtual Archive& operator>>(int32_t& val) = 0;
        virtual Archive& operator<<(float val) = 0;
        virtual Archive& operator>>(float& val) = 0;
        virtual Archive& operator<<(const std::string& val) = 0;
        virtual Archive& operator>>(std::string& val) = 0;

        // uint32_t / uint8_t — delegate to int32_t
        Archive& operator<<(uint32_t val)       { return (*this) << static_cast<int32_t>(val); }
        Archive& operator>>(uint32_t& val)      { int32_t tmp; (*this) >> tmp; val = static_cast<uint32_t>(tmp); return *this; }
        Archive& operator<<(uint8_t val)        { return (*this) << static_cast<int32_t>(val); }
        Archive& operator>>(uint8_t& val)       { int32_t tmp; (*this) >> tmp; val = static_cast<uint8_t>(tmp); return *this; }


        template<typename T>
        void write(const T& val) {
            WriteBytes(&val, sizeof(T));
        }

        template<typename T>
        void read(T& val) {
            ReadBytes(&val, sizeof(T));
        }

        virtual void writeBytes(const void* data, size_t size) = 0;
        virtual void readBytes(void* data, size_t size) = 0;
    };

    enum class ArchiveMode {
        read,
        write
    };

    class VE_API BinaryArchive : public Archive {
    public:
        BinaryArchive(const std::string& filename, ArchiveMode mode);
        ~BinaryArchive();

        Archive& operator<<(int32_t val) override;
        Archive& operator>>(int32_t& val) override;
        Archive& operator<<(float val) override;
        Archive& operator>>(float& val) override;
        Archive& operator<<(const std::string& val) override;
        Archive& operator>>(std::string& val) override;

        void writeBytes(const void* data, size_t size) override;
        void readBytes(void* data, size_t size) override;

        bool isGood() const { return m_isGood; }
        void flush();

    private:
        template<typename T>
        void write(T val);

        template<typename T>
        void read(T& val);

        void writeString(const std::string& str);
        void readString(std::string& str);

        std::fstream m_file;
        ArchiveMode m_mode;
        bool m_isGood;
        std::vector<uint8_t> m_buffer;
        size_t m_bufferPos;
    };

    class TextArchive : public Archive {
    public:
        TextArchive(const std::string& filename, ArchiveMode mode) {
            if (mode == ArchiveMode::write) {
                std::filesystem::path filePath(filename);
                if (filePath.has_parent_path()) {
                    std::filesystem::create_directories(filePath.parent_path());
                }
                m_out.open(filename);
                m_isGood = m_out.is_open();
            }
            else if (vfs::isArchived(filename)) {
                // Loose text is read in text mode, which folds CRLF to LF; apply
                // the same fold to packed bytes so string fields match a loose read.
                std::string text;
                if (vfs::readText(filename, text)) {
                    std::string normalized;
                    normalized.reserve(text.size());
                    for (size_t i = 0; i < text.size(); ++i) {
                        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                            continue;
                        normalized.push_back(text[i]);
                    }
                    m_memIn.str(normalized);
                    m_useMemory = true;
                    m_isGood = true;
                }
            }
            else {
                m_in.open(filename);
                m_isGood = m_in.is_open();
            }
        }

        ~TextArchive() {
            if (m_out.is_open()) m_out.close();
            if (m_in.is_open()) m_in.close();
        }

        Archive& operator<<(int32_t val) override {
            if (m_out.is_open()) m_out << val << " ";
            return *this;
        }

        Archive& operator>>(int32_t& val) override {
            if (readable()) in() >> val;
            return *this;
        }

        Archive& operator<<(float val) override {
            if (m_out.is_open()) m_out << val << " ";
            return *this;
        }

        Archive& operator>>(float& val) override {
            if (readable()) in() >> val;
            return *this;
        }

        Archive& operator<<(const std::string& val) override {
            if (m_out.is_open()) {
                m_out << val.size() << " ";
                m_out.write(val.data(), val.size());
                m_out << " ";
            }
            return *this;
        }

        Archive& operator>>(std::string& val) override {
            if (readable()) {
                std::istream& src = in();
                size_t len = 0;
                src >> len;
                src.get();
                val.resize(len);
                src.read(&val[0], static_cast<std::streamsize>(len));
            }
            return *this;
        }

        void writeBytes(const void* data, size_t size) override {
            // Text format doesn't support raw bytes
            (void)data;
            (void)size;
        }

        void readBytes(void* data, size_t size) override {
            // Text format doesn't support raw bytes
            (void)data;
            (void)size;
        }

        bool isGood() const { return m_isGood; }

    private:
        bool readable() const { return m_useMemory || m_in.is_open(); }
        std::istream& in() { return m_useMemory ? static_cast<std::istream&>(m_memIn)
                                                : static_cast<std::istream&>(m_in); }

        std::ofstream m_out;
        std::ifstream m_in;
        std::istringstream m_memIn;
        bool m_useMemory = false;
        bool m_isGood = false;
    };

    // --- glm vector serialization helpers (delegate to float ops) ---
    inline Archive& operator<<(Archive& ar, const glm::vec2& v) {
        ar << v.x << v.y;
        return ar;
    }
    inline Archive& operator>>(Archive& ar, glm::vec2& v) {
        ar >> v.x >> v.y;
        return ar;
    }
    inline Archive& operator<<(Archive& ar, const glm::vec3& v) {
        ar << v.x << v.y << v.z;
        return ar;
    }
    inline Archive& operator>>(Archive& ar, glm::vec3& v) {
        ar >> v.x >> v.y >> v.z;
        return ar;
    }
    inline Archive& operator<<(Archive& ar, const glm::vec4& v) {
        ar << v.x << v.y << v.z << v.w;
        return ar;
    }
    inline Archive& operator>>(Archive& ar, glm::vec4& v) {
        ar >> v.x >> v.y >> v.z >> v.w;
        return ar;
    }

}

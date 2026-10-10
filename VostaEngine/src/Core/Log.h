#ifndef LOG_H
#define LOG_H

#include <string>
#include <chrono>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <mutex>

#include "Core.h"

namespace ve {

    enum class LogLevel {
        Success,
        Warn,
        Error
    };

    struct LogEntry {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;

        LogEntry(LogLevel lvl, std::string msg)
            : level(lvl)
            , message(std::move(msg))
            , timestamp(std::chrono::system_clock::now())
        {
        }
    };

	class VE_API Logger {
	public:

        static Logger& Get() {
            static Logger instance;
            return instance;
        }

		void success(const std::string& message);
		void warn(const std::string& message);
		void error(const std::string& message);

        template<typename... Args>
        void success(const char* fmt, Args... args) {
            std::lock_guard lock(m_mutex);
            m_logs.emplace_back(LogLevel::Success, formatString(fmt, args...));
        }

        template<typename... Args>
        void warn(const char* fmt, Args... args) {
            std::lock_guard lock(m_mutex);
            m_logs.emplace_back(LogLevel::Warn, formatString(fmt, args...));
        }

        template<typename... Args>
        void error(const char* fmt, Args... args) {
            std::lock_guard lock(m_mutex);
            m_logs.emplace_back(LogLevel::Error, formatString(fmt, args...));
        }


        void printAllToConsole();

        template<typename... Args>
        std::string formatString(const char* fmt, Args... args) {
          
            int size = std::snprintf(nullptr, 0, fmt, args...);
            if (size <= 0) return "";

            std::string result(size + 1, '\0');
            std::snprintf(&result[0], size + 1, fmt, args...);
            result.resize(size); 
            return result;
        }

	private:
		std::vector<LogEntry> m_logs;
		std::mutex m_mutex;

	};


}

#define VE_CORE_WARNING(...) \
    do { \
        std::stringstream _ss; \
        _ss << __VA_ARGS__; \
        ve::Logger::Get().warning(_ss.str()); \
    } while(0)

#define VE_CORE_ERROR(...) \
    do { \
        std::stringstream _ss; \
        _ss << __VA_ARGS__; \
        ve::Logger::Get().error(_ss.str()); \
    } while(0)

#define VE_CORE_SUCCESS(...) \
    do { \
        std::stringstream _ss; \
        _ss << __VA_ARGS__; \
        ve::Logger::Get().success(_ss.str()); \
    } while(0)

#define VE_CORE_INFO_PRINT(...) \
    do { \
        std::string _msg = ve::Logger::Get().formatString(__VA_ARGS__); \
        ve::Logger::Get().success(_msg); \
        std::cout << "[INFO] " << _msg << std::endl; \
    } while(0)

#define VE_CORE_SUCCESS_PRINT(...) \
    do { \
        std::string _msg = ve::Logger::Get().formatString(__VA_ARGS__); \
        ve::Logger::Get().success(_msg); \
        std::cerr << "\033[32m"<<"[SUCCESS] " << _msg <<"\033[0m"<< std::endl; \
    } while(0)

#define VE_CORE_WARN_PRINT(...) \
    do { \
        std::string _msg = ve::Logger::Get().formatString(__VA_ARGS__); \
        ve::Logger::Get().warn(_msg); \
        std::cerr << "\033[33m"<<"[WARNING] " << _msg <<"\033[0m"<< std::endl; \
    } while(0)

#define VE_CORE_ERROR_PRINT(...) \
    do { \
        std::string _msg = ve::Logger::Get().formatString(__VA_ARGS__); \
        ve::Logger::Get().error(_msg); \
        std::cerr << "\033[31m"<<"[ERROR] " << _msg <<"\033[0m"<< std::endl; \
    } while(0)


#endif // !LOG_H

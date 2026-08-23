#include "vepch.h"
#include "Log.h"

void ve::Logger::success(const std::string& message) {
    m_logs.emplace_back(LogLevel::Success, message);
}

void ve::Logger::warn(const std::string& message) {
    m_logs.emplace_back(LogLevel::Warn, message);
}

void ve::Logger::error(const std::string& message) {
    m_logs.emplace_back(LogLevel::Error, message);
}

void ve::Logger::printAllToConsole() {
    for (const auto& entry : m_logs) {
        const char* prefix = "";
        switch (entry.level) {
        case LogLevel::Success:
            prefix = "[SUCCESS] ";
            std::cout << "\033[32m";  
            break;
        case LogLevel::Warn:
            prefix = "[WARN] ";
            std::cout << "\033[33m";  
            break;
        case LogLevel::Error:
            prefix = "[ERROR] ";
            std::cout << "\033[31m";  
            break;
        }

        std::cout << prefix << entry.message;

        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        struct tm timeInfo;
        localtime_s(&timeInfo, &time_t);

        std::cout << " \033[90m("
            << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
            << ")\033[0m" << std::endl;

    }
}



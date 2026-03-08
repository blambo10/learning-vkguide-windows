#pragma once
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/color.h>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    template<typename... Args>
    static void log(LogLevel level, fmt::format_string<Args...> fmt_str, Args&&... args) {
        auto msg = fmt::format(fmt_str, std::forward<Args>(args)...);

        switch (level) {
        case LogLevel::DEBUG:
            fmt::println("[DEBUG] {}", msg); break;
        case LogLevel::INFO:
            fmt::println(fmt::fg(fmt::color::green), "[INFO]  {}", msg); break;
        case LogLevel::WARN:
            fmt::println(fmt::fg(fmt::color::yellow), "[WARN]  {}", msg); break;
        case LogLevel::ERROR:
            fmt::println(stderr, fmt::fg(fmt::color::red), "[ERROR] {}", msg); break;
        }
    }
};

#define LOG_DEBUG(...) Logger::log(LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  Logger::log(LogLevel::INFO,  __VA_ARGS__)
#define LOG_WARN(...)  Logger::log(LogLevel::WARN,  __VA_ARGS__)
#define LOG_ERROR(...) Logger::log(LogLevel::ERROR, __VA_ARGS__)

// Usage
//LOG_INFO("Picked GPU: {}", deviceName);
//LOG_WARN("Validation layer not found: {}", layerName);
//LOG_ERROR("Swapchain creation failed: {}", result);
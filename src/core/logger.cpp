#include "sentinel/core/logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>

namespace sentinel::core {

void Logger::Init(const std::string& level, const std::string& log_file) {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        sinks.push_back(console_sink);
        
        // File sink (if configured)
        if (!log_file.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
            file_sink->set_level(spdlog::level::trace);
            sinks.push_back(file_sink);
        }
        
        auto logger = std::make_shared<spdlog::logger>("sentinel", sinks.begin(), sinks.end());
        
        spdlog::set_default_logger(logger);
        
        spdlog::level::level_enum log_level = spdlog::level::info;
        if (level == "debug") log_level = spdlog::level::debug;
        else if (level == "trace") log_level = spdlog::level::trace;
        else if (level == "warn") log_level = spdlog::level::warn;
        else if (level == "error") log_level = spdlog::level::err;
        
        spdlog::set_level(log_level);
        spdlog::info("Logger initialized at level: {}", level);
    } catch (const spdlog::spdlog_ex& ex) {
        printf("Log init failed: %s\n", ex.what());
    }
}

} // namespace sentinel::core

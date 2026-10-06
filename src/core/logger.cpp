#include "sentinel/core/logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>

namespace sentinel::core {

namespace {

constexpr std::size_t kMaxLogSize = 10 * 1024 * 1024; // 10 MiB per file
constexpr std::size_t kMaxLogFiles = 3;

spdlog::level::level_enum ParseLevel(const std::string& level) {
    if (level == "trace") return spdlog::level::trace;
    if (level == "debug") return spdlog::level::debug;
    if (level == "info")  return spdlog::level::info;
    if (level == "warn")  return spdlog::level::warn;
    if (level == "error") return spdlog::level::err;
    if (level == "critical") return spdlog::level::critical;
    spdlog::warn("Unknown log level '{}', falling back to 'info'", level);
    return spdlog::level::info;
}

void Install(std::vector<spdlog::sink_ptr> sinks, const std::string& level) {
    auto logger = std::make_shared<spdlog::logger>("sentinel", sinks.begin(), sinks.end());
    logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(logger);
    spdlog::set_level(ParseLevel(level));
}

} // namespace

void Logger::InitConsole() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    Install({console_sink}, "info");
}

void Logger::Init(const std::string& level, const std::string& log_file) {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        sinks.push_back(console_sink);

        if (!log_file.empty()) {
            // Rotating rather than basic: the previous sink truncated the log on
            // every init and then grew without bound.
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file, kMaxLogSize, kMaxLogFiles);
            file_sink->set_level(spdlog::level::trace);
            sinks.push_back(file_sink);
        }

        Install(std::move(sinks), level);
        spdlog::info("Logger initialized at level: {}", level);
    } catch (const spdlog::spdlog_ex& ex) {
        // The bootstrap console logger is still installed at this point.
        spdlog::error("Log init failed ({}); continuing with console logging", ex.what());
    }
}

void Logger::SetLevel(const std::string& level) {
    spdlog::set_level(ParseLevel(level));
}

} // namespace sentinel::core

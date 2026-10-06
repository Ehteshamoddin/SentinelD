#pragma once

#include <string>
#include <spdlog/spdlog.h>

namespace sentinel::core {

class Logger {
public:
    // Console-only bootstrap logger, so failures before the real configuration
    // is known are still visible. Must be called before any thread is spawned.
    static void InitConsole();

    // Installs the final logger (console + optional rotating file sink).
    // Swapping the default logger is not safe against concurrent logging, so
    // this must also be called before any thread is spawned.
    static void Init(const std::string& level, const std::string& log_file = "");

    // Safe to call at any time, including while worker threads are logging.
    // This is what config reload uses instead of rebuilding the logger.
    static void SetLevel(const std::string& level);
};

} // namespace sentinel::core

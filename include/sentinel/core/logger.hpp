#pragma once

#include <string>
#include <spdlog/spdlog.h>

namespace sentinel::core {

class Logger {
public:
    static void Init(const std::string& level = "info", const std::string& log_file = "");
};

} // namespace sentinel::core

#pragma once

#include <string>

namespace sentinel::daemon {

class Daemonizer {
public:
    static void Daemonize(const std::string& pid_file);
    static bool IsRunning(const std::string& pid_file);
};

} // namespace sentinel::daemon

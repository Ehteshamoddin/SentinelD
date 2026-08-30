#pragma once

#include <string>

namespace sentinel::daemon {

class Daemonizer {
public:
    // Detaches from the controlling terminal and closes inherited descriptors.
    // Must run BEFORE any log file is opened: this closes every descriptor it
    // does not own, including one a file sink may already be holding.
    static void Daemonize();

    // Creates the PID file and holds an exclusive advisory lock on it for the
    // process lifetime. Returns false if another instance already holds it.
    // The kernel drops the lock automatically if the process dies, so a stale
    // PID file left by a crash never blocks a restart.
    static bool AcquirePidFile(const std::string& pid_file);

    // Unlinks the PID file and releases the lock. Safe to call if unacquired.
    static void ReleasePidFile();
};

} // namespace sentinel::daemon

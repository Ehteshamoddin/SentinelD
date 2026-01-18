#include "sentinel/daemon/daemonizer.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <csignal>
#include <cstdlib>

namespace sentinel::daemon {

void Daemonizer::Daemonize(const std::string& pid_file) {
    spdlog::info("Daemonizing...");

    // 1. Fork off the parent process
    pid_t pid = fork();
    if (pid < 0) {
        spdlog::critical("Failed to fork: {}", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        // Parent exits
        exit(EXIT_SUCCESS);
    }

    // 2. Create a new session ID for the child process
    if (setsid() < 0) {
        spdlog::critical("Failed to setsid: {}", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // 3. Catch, ignore and handle signals (handled by SignalHandler later)
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    // 4. Fork off for the second time
    pid = fork();
    if (pid < 0) {
        spdlog::critical("Failed to fork (2): {}", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // 5. Set new file permissions
    umask(0);

    // 6. Change the working directory to the root directory
    if (chdir("/") < 0) {
        spdlog::error("Failed to chdir to /: {}", strerror(errno));
        // Not critical, continue
    }

    // 7. Close all open file descriptors
    /* 
    // In a real production app, we should close all FDs. 
    // For now, we just redirect stdio to /dev/null
    */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    // Redirect stdin/out/err to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) close(fd);
    }

    // 8. Write PID file
    std::ofstream pf(pid_file);
    if (pf.is_open()) {
        pf << getpid();
        pf.close();
    } else {
        // Can't log to stdout anymore, so this is silent failure unless we have syslog/file log
    }
}

bool Daemonizer::IsRunning(const std::string& pid_file) {
    std::ifstream pf(pid_file);
    if (!pf.is_open()) return false;
    
    pid_t pid;
    pf >> pid;
    
    // Check if process exists using kill(pid, 0)
    if (kill(pid, 0) == 0) {
        return true;
    }
    return false;
}

} // namespace sentinel::daemon

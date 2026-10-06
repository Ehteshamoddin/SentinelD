#include "sentinel/daemon/daemonizer.hpp"

#include <spdlog/spdlog.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace sentinel::daemon {

namespace {

int g_pid_fd = -1;
std::string g_pid_path;

std::string ErrText(int err) {
    return std::generic_category().message(err);
}

// Closes every descriptor from 3 upward. The previous implementation looped
// down from sysconf(_SC_OPEN_MAX), which can be 1048576 -- a million syscalls --
// and also closed 0/1/2 before reopening them.
void CloseInheritedDescriptors() {
#if defined(SYS_close_range)
    if (syscall(SYS_close_range, 3u, ~0u, 0) == 0) {
        return;
    }
#endif
    long max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd < 0 || max_fd > 4096) {
        max_fd = 4096;
    }
    for (long fd = 3; fd < max_fd; ++fd) {
        close(static_cast<int>(fd));
    }
}

} // namespace

void Daemonizer::Daemonize() {
    spdlog::info("Daemonizing...");

    // 1. Fork so the parent exits and the child is not a process group leader,
    //    which is a precondition for setsid().
    pid_t pid = fork();
    if (pid < 0) {
        spdlog::critical("Failed to fork: {}", ErrText(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        // _exit(), not exit(): the child inherited copies of our stdio buffers,
        // and running atexit handlers here would flush them a second time.
        _exit(EXIT_SUCCESS);
    }

    // 2. New session, detached from any controlling terminal.
    if (setsid() < 0) {
        spdlog::critical("Failed to setsid: {}", ErrText(errno));
        _exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN); // re-registered by SignalHandler::Init() below

    // 3. Fork again so the daemon is not a session leader and can therefore
    //    never reacquire a controlling terminal.
    pid = fork();
    if (pid < 0) {
        spdlog::critical("Failed to fork (2): {}", ErrText(errno));
        _exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        _exit(EXIT_SUCCESS);
    }

    umask(0);

    if (chdir("/") < 0) {
        spdlog::error("Failed to chdir to /: {}", ErrText(errno));
    }

    CloseInheritedDescriptors();

    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }
}

bool Daemonizer::AcquirePidFile(const std::string& pid_file) {
    int fd = open(pid_file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) {
        spdlog::critical("Failed to open PID file {}: {}", pid_file, ErrText(errno));
        return false;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        int err = errno;
        close(fd);
        if (err == EWOULDBLOCK) {
            spdlog::critical("Another instance is already running (lock held on {})", pid_file);
        } else {
            spdlog::critical("Failed to lock PID file {}: {}", pid_file, ErrText(err));
        }
        return false;
    }

    if (ftruncate(fd, 0) < 0) {
        spdlog::error("Failed to truncate PID file {}: {}", pid_file, ErrText(errno));
    }

    const std::string contents = std::to_string(getpid()) + "\n";
    if (write(fd, contents.data(), contents.size()) < 0) {
        spdlog::error("Failed to write PID file {}: {}", pid_file, ErrText(errno));
    }

    // The descriptor stays open on purpose: closing it would drop the lock.
    g_pid_fd = fd;
    g_pid_path = pid_file;
    spdlog::debug("Acquired PID file {} (pid {})", pid_file, getpid());
    return true;
}

void Daemonizer::ReleasePidFile() {
    if (g_pid_fd < 0) {
        return;
    }
    if (unlink(g_pid_path.c_str()) < 0 && errno != ENOENT) {
        spdlog::error("Failed to remove PID file {}: {}", g_pid_path, ErrText(errno));
    }
    close(g_pid_fd); // releases the flock
    g_pid_fd = -1;
    g_pid_path.clear();
}

} // namespace sentinel::daemon

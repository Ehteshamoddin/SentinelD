#include "sentinel/daemon/signal_handler.hpp"

#include <spdlog/spdlog.h>
#include <cstring>
#include <cerrno>
#include <system_error>
#include <pthread.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace sentinel::daemon {

std::atomic<bool> SignalHandler::exit_flag_{false};
std::atomic<bool> SignalHandler::reload_flag_{false};
int SignalHandler::wake_fd_{-1};

namespace {

const int kHandledSignals[] = {SIGINT, SIGTERM, SIGHUP};

std::string ErrText(int err) {
    // strerror() is not thread-safe; system_category().message() is.
    return std::generic_category().message(err);
}

} // namespace

bool SignalHandler::Init() {
    wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake_fd_ < 0) {
        spdlog::critical("Failed to create wakeup eventfd: {}", ErrText(errno));
        return false;
    }

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = HandleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    for (int signum : kHandledSignals) {
        if (sigaction(signum, &sa, nullptr) == -1) {
            spdlog::critical("Failed to register handler for signal {}: {}",
                             signum, ErrText(errno));
            return false;
        }
    }

    spdlog::debug("Signal handlers registered");
    return true;
}

void SignalHandler::HandleSignal(int signum) {
    switch (signum) {
        case SIGINT:
        case SIGTERM:
            exit_flag_.store(true);
            break;
        case SIGHUP:
            reload_flag_.store(true);
            break;
        default:
            return;
    }

    // write() is async-signal-safe; this is what actually wakes the main loop.
    if (wake_fd_ >= 0) {
        const uint64_t one = 1;
        ssize_t ignored = write(wake_fd_, &one, sizeof(one));
        (void)ignored;
    }
}

bool SignalHandler::ShouldExit()   { return exit_flag_.load(); }
bool SignalHandler::ShouldReload() { return reload_flag_.load(); }
void SignalHandler::ResetReload()  { reload_flag_.store(false); }

void SignalHandler::Wait(int timeout_ms) {
    if (wake_fd_ < 0) {
        return;
    }

    struct pollfd pfd = {wake_fd_, POLLIN, 0};
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret < 0) {
        if (errno != EINTR) {
            spdlog::error("poll() failed while waiting: {}", ErrText(errno));
        }
        return;
    }

    if (ret > 0 && (pfd.revents & POLLIN)) {
        // Drain the counter. A signal landing between the flag check and the
        // poll() is not lost: eventfd is a counter, so poll() returns at once.
        uint64_t value = 0;
        ssize_t ignored = read(wake_fd_, &value, sizeof(value));
        (void)ignored;
    }
}

ScopedSignalBlock::ScopedSignalBlock() {
    sigset_t block;
    sigemptyset(&block);
    for (int signum : kHandledSignals) {
        sigaddset(&block, signum);
    }
    if (pthread_sigmask(SIG_BLOCK, &block, &previous_) == 0) {
        active_ = true;
    }
}

ScopedSignalBlock::~ScopedSignalBlock() {
    if (active_) {
        pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
    }
}

} // namespace sentinel::daemon

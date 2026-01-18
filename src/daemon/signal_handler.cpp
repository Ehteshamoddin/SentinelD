#include "sentinel/daemon/signal_handler.hpp"
#include <csignal>
#include <spdlog/spdlog.h>
#include <cstring> // for memset

namespace sentinel::daemon {

std::atomic<bool> SignalHandler::exit_flag_{false};
std::atomic<bool> SignalHandler::reload_flag_{false};

void SignalHandler::Init() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = HandleSignal;
    sigemptyset(&sa.sa_mask);
    
    // Restart system calls if possible
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        spdlog::error("Failed to register SIGINT handler");
    }
    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        spdlog::error("Failed to register SIGTERM handler");
    }
    if (sigaction(SIGHUP, &sa, nullptr) == -1) {
        spdlog::error("Failed to register SIGHUP handler");
    }
    
    spdlog::debug("Signal handlers registered");
}

void SignalHandler::HandleSignal(int signum) {
    switch (signum) {
        case SIGINT:
        case SIGTERM:
            exit_flag_ = true;
            break;
        case SIGHUP:
            reload_flag_ = true;
            break;
        default:
            break;
    }
}

bool SignalHandler::ShouldExit() {
    return exit_flag_.load();
}

bool SignalHandler::ShouldReload() {
    return reload_flag_.load();
}

void SignalHandler::ResetReload() {
    reload_flag_.store(false);
}

} // namespace sentinel::daemon

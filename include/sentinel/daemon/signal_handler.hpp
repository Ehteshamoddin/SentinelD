#pragma once

#include <atomic>
#include <signal.h>

namespace sentinel::daemon {

class SignalHandler {
public:
    // Registers handlers and creates the wakeup descriptor. Must be called
    // after Daemonize(), which closes inherited descriptors.
    static bool Init();

    static bool ShouldExit();
    static bool ShouldReload();
    static void ResetReload();

    // Sleeps up to timeout_ms, returning immediately when a signal arrives.
    // This replaces sleep_for(), which completes its full duration even after
    // a handler has run and therefore delayed shutdown by a whole interval.
    static void Wait(int timeout_ms);

private:
    static void HandleSignal(int signum);

    static std::atomic<bool> exit_flag_;
    static std::atomic<bool> reload_flag_;
    static int wake_fd_;
};

// Blocks the daemon's signals in the calling thread for the object's lifetime.
// Constructing one around a std::thread launch makes the new thread inherit the
// mask, so process-directed signals are always delivered to the main thread
// rather than to an arbitrary worker.
class ScopedSignalBlock {
public:
    ScopedSignalBlock();
    ~ScopedSignalBlock();

    ScopedSignalBlock(const ScopedSignalBlock&) = delete;
    ScopedSignalBlock& operator=(const ScopedSignalBlock&) = delete;

private:
    sigset_t previous_{};
    bool active_{false};
};

} // namespace sentinel::daemon

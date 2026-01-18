#pragma once

#include <functional>
#include <atomic>

namespace sentinel::daemon {

class SignalHandler {
public:
    using SignalCallback = std::function<void(int)>;

    static void Init();
    static bool ShouldExit();
    static bool ShouldReload();
    static void ResetReload();

private:
    static void HandleSignal(int signum);
    
    static std::atomic<bool> exit_flag_;
    static std::atomic<bool> reload_flag_;
};

} // namespace sentinel::daemon

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <sys/inotify.h>

namespace sentinel::features {

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    // `events` accepts: create, delete, modify, close_write, move, attrib.
    // An empty or fully unrecognised list falls back to a sensible default.
    void Start(const std::string& path, const std::vector<std::string>& events);
    void Stop();

private:
    void WatchLoop();
    void HandleEvent(const struct inotify_event* event);

    int inotify_fd_{-1};
    int watch_desc_{-1};
    int stop_fd_{-1};   // eventfd, so Stop() wakes the thread immediately
    std::string watch_path_;
    std::atomic<bool> running_{false};
    std::thread watcher_thread_;
};

// Exposed for clarity and future testing; maps config strings to an inotify mask.
uint32_t MaskFromEventNames(const std::vector<std::string>& events);

} // namespace sentinel::features

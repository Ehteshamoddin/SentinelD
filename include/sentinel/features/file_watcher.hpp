#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <sys/inotify.h>

namespace sentinel::features {

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    void Start(const std::string& path, const std::vector<std::string>& events);
    void Stop();

private:
    void WatchLoop();
    void HandleEvent(const struct inotify_event* event);

    int inotify_fd_{-1};
    int watch_desc_{-1};
    std::string watch_path_;
    std::atomic<bool> running_{false};
    std::thread watcher_thread_;
};

} // namespace sentinel::features

#include "sentinel/features/file_watcher.hpp"
#include <spdlog/spdlog.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>

namespace sentinel::features {

FileWatcher::FileWatcher() {
    inotify_fd_ = inotify_init();
    if (inotify_fd_ < 0) {
        spdlog::error("Failed to initialize inotify: {}", strerror(errno));
    }
}

FileWatcher::~FileWatcher() {
    Stop();
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
    }
}

void FileWatcher::Start(const std::string& path, const std::vector<std::string>& /*events*/) {
    if (inotify_fd_ < 0) return;
    if (running_) return;

    watch_path_ = path;
    
    // Monitor all common file events
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM;
    
    watch_desc_ = inotify_add_watch(inotify_fd_, path.c_str(), mask);
    if (watch_desc_ < 0) {
        spdlog::error("Failed to add watch for {}: {}", path, strerror(errno));
        return;
    }

    running_ = true;
    watcher_thread_ = std::thread(&FileWatcher::WatchLoop, this);
    spdlog::info("Started FileWatcher on {}", path);
}

void FileWatcher::Stop() {
    if (!running_) return;
    running_ = false;
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    if (watch_desc_ >= 0) {
        inotify_rm_watch(inotify_fd_, watch_desc_);
        watch_desc_ = -1;
    }
}

void FileWatcher::WatchLoop() {
    constexpr size_t BUF_LEN = (10 * (sizeof(struct inotify_event) + NAME_MAX + 1));
    char buf[BUF_LEN];
    struct pollfd pfd = { inotify_fd_, POLLIN, 0 };

    while (running_) {
        // Poll with 1 second timeout to check running_ flag
        int ret = poll(&pfd, 1, 1000);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            spdlog::error("Poll error in FileWatcher: {}", strerror(errno));
            break;
        }
        
        if (ret == 0) continue; // Timeout

        if (pfd.revents & POLLIN) {
            ssize_t len = read(inotify_fd_, buf, BUF_LEN);
            if (len < 0) {
                if (errno == EINTR) continue;
                spdlog::error("Read error in FileWatcher: {}", strerror(errno));
                break;
            }

            const struct inotify_event* event;
            for (char* ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
                event = (const struct inotify_event*) ptr;
                HandleEvent(event);
            }
        }
    }
}

void FileWatcher::HandleEvent(const struct inotify_event* event) {
    if (event->len) {
        std::string name = event->name;
        std::string type;
        
        if (event->mask & IN_CREATE) type = "CREATED";
        else if (event->mask & IN_DELETE) type = "DELETED";
        else if (event->mask & IN_MODIFY) type = "MODIFIED";
        else if (event->mask & IN_MOVED_TO) type = "MOVED_TO";
        else if (event->mask & IN_MOVED_FROM) type = "MOVED_FROM";
        else type = "UNKNOWN";

        spdlog::warn("FILE ALERT: {} was {} in {}", name, type, watch_path_);
    }
}

} // namespace sentinel::features

#include "sentinel/features/file_watcher.hpp"
#include "sentinel/daemon/signal_handler.hpp"

#include <spdlog/spdlog.h>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <climits>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>

namespace sentinel::features {

namespace {

constexpr uint32_t kDefaultMask =
    IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM;

std::string ErrText(int err) {
    return std::generic_category().message(err);
}

} // namespace

uint32_t MaskFromEventNames(const std::vector<std::string>& events) {
    if (events.empty()) {
        return kDefaultMask;
    }

    uint32_t mask = 0;
    for (const auto& name : events) {
        if (name == "create")           mask |= IN_CREATE;
        else if (name == "delete")      mask |= IN_DELETE | IN_DELETE_SELF;
        else if (name == "modify")      mask |= IN_MODIFY;
        else if (name == "close_write") mask |= IN_CLOSE_WRITE;
        else if (name == "move")        mask |= IN_MOVED_TO | IN_MOVED_FROM | IN_MOVE_SELF;
        else if (name == "attrib")      mask |= IN_ATTRIB;
        else spdlog::warn("Unknown watcher event '{}', ignoring", name);
    }

    if (mask == 0) {
        spdlog::warn("No recognised watcher events configured; using defaults");
        return kDefaultMask;
    }
    return mask;
}

FileWatcher::FileWatcher() {
    // IN_CLOEXEC so the descriptor does not leak into any child we exec;
    // IN_NONBLOCK so a spurious poll() wakeup can never block the thread.
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd_ < 0) {
        spdlog::error("Failed to initialize inotify: {}", ErrText(errno));
    }

    stop_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (stop_fd_ < 0) {
        spdlog::error("Failed to create watcher stop eventfd: {}", ErrText(errno));
    }
}

FileWatcher::~FileWatcher() {
    Stop();
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
        inotify_fd_ = -1;
    }
    if (stop_fd_ >= 0) {
        close(stop_fd_);
        stop_fd_ = -1;
    }
}

void FileWatcher::Start(const std::string& path, const std::vector<std::string>& events) {
    if (inotify_fd_ < 0) return;
    if (running_) return;

    if (path.empty()) {
        spdlog::warn("No watcher path configured; file watching disabled");
        return;
    }

    const uint32_t mask = MaskFromEventNames(events);

    watch_desc_ = inotify_add_watch(inotify_fd_, path.c_str(), mask);
    if (watch_desc_ < 0) {
        spdlog::error("Failed to add watch for {}: {}", path, ErrText(errno));
        return;
    }

    watch_path_ = path;
    running_ = true;

    // Spawn with the daemon's signals blocked so the worker inherits the mask
    // and process-directed signals always land on the main thread.
    {
        daemon::ScopedSignalBlock block;
        watcher_thread_ = std::thread(&FileWatcher::WatchLoop, this);
    }

    spdlog::info("Started FileWatcher on {} (mask 0x{:x})", path, mask);
}

void FileWatcher::Stop() {
    running_ = false;
    if (stop_fd_ >= 0) {
        const uint64_t one = 1;
        ssize_t ignored = write(stop_fd_, &one, sizeof(one));
        (void)ignored;
    }
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    // Reclaim the watch even if the thread was never started, which is the
    // case when inotify_add_watch succeeded but Start() bailed out afterwards.
    if (watch_desc_ >= 0 && inotify_fd_ >= 0) {
        inotify_rm_watch(inotify_fd_, watch_desc_);
    }
    watch_desc_ = -1;

    // Drain the eventfd so a subsequent Start() (config reload) is not
    // immediately told to stop again.
    if (stop_fd_ >= 0) {
        uint64_t value = 0;
        ssize_t ignored = read(stop_fd_, &value, sizeof(value));
        (void)ignored;
    }
}

void FileWatcher::WatchLoop() {
    constexpr size_t kBufLen = 10 * (sizeof(struct inotify_event) + NAME_MAX + 1);
    // inotify records must be read into suitably aligned storage; a bare
    // char[] and a reinterpret_cast is undefined on strict-alignment targets.
    alignas(struct inotify_event) char buf[kBufLen];

    // Watching the stop eventfd alongside inotify makes shutdown immediate;
    // the 1s timeout is now only a backstop, not the shutdown latency.
    struct pollfd pfds[2] = {
        {inotify_fd_, POLLIN, 0},
        {stop_fd_,    POLLIN, 0},
    };
    const nfds_t nfds = (stop_fd_ >= 0) ? 2 : 1;

    while (running_) {
        int ret = poll(pfds, nfds, 1000);

        if (ret < 0) {
            if (errno == EINTR) continue;
            spdlog::error("Poll error in FileWatcher: {}", ErrText(errno));
            break;
        }
        if (ret == 0) continue;
        if (nfds == 2 && (pfds[1].revents & POLLIN)) {
            break; // Stop() was called
        }
        if (!(pfds[0].revents & POLLIN)) continue;

        ssize_t len = read(inotify_fd_, buf, kBufLen);
        if (len < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            spdlog::error("Read error in FileWatcher: {}", ErrText(errno));
            break;
        }
        if (len == 0) continue;

        char* ptr = buf;
        char* end = buf + len;
        while (ptr + sizeof(struct inotify_event) <= end) {
            const auto* event = reinterpret_cast<const struct inotify_event*>(ptr);
            const size_t record = sizeof(struct inotify_event) + event->len;
            if (ptr + record > end) break; // defensive: truncated record
            HandleEvent(event);
            ptr += record;
        }
    }
}

void FileWatcher::HandleEvent(const struct inotify_event* event) {
    if (event->mask & IN_Q_OVERFLOW) {
        // Events were dropped by the kernel and cannot be recovered. A real
        // FIM would trigger a full rescan here; at minimum it must be visible.
        spdlog::error("inotify queue overflow: filesystem events were lost");
        return;
    }

    if (event->mask & IN_IGNORED) {
        return; // watch removed; Stop() or the directory went away
    }

    // A single event can carry several bits, so this must not be an else-if
    // chain, and len == 0 means the event refers to the watched directory
    // itself rather than to a file inside it.
    std::string types;
    auto add = [&types](const char* name) {
        if (!types.empty()) types += "|";
        types += name;
    };
    if (event->mask & IN_CREATE)      add("CREATED");
    if (event->mask & IN_DELETE)      add("DELETED");
    if (event->mask & IN_MODIFY)      add("MODIFIED");
    if (event->mask & IN_CLOSE_WRITE) add("CLOSED_WRITE");
    if (event->mask & IN_MOVED_TO)    add("MOVED_TO");
    if (event->mask & IN_MOVED_FROM)  add("MOVED_FROM");
    if (event->mask & IN_ATTRIB)      add("ATTRIB");
    if (event->mask & IN_DELETE_SELF) add("WATCHED_DIR_DELETED");
    if (event->mask & IN_MOVE_SELF)   add("WATCHED_DIR_MOVED");
    if (types.empty()) {
        types = "UNKNOWN";
    }

    const bool is_dir = (event->mask & IN_ISDIR) != 0;

    if (event->len > 0) {
        const std::string name(event->name, ::strnlen(event->name, event->len));
        spdlog::warn("FILE ALERT: {}{} was {} in {}",
                     name, is_dir ? " (dir)" : "", types, watch_path_);
    } else {
        spdlog::warn("FILE ALERT: watched path {} was {}", watch_path_, types);
    }
}

} // namespace sentinel::features

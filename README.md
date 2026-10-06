# sentineld - Linux System Monitor Daemon

`sentineld` is a production-quality C++ daemon designed for Linux systems. It monitors system resources (CPU, Memory) and file integrity (using inotify).

## Features

- **Daemonization**: Runs as a background process with PID file management.
- **Signal Handling**: Graceful shutdown (SIGTERM/SIGINT) and configuration reload (SIGHUP).
- **Resource Monitoring**: Tracks CPU and Memory usage via `/proc`, alerting on high load.
- **File Integrity Monitoring**: Watches critical directories for file creation, modification, and deletion events.
- **Structured Logging**: Uses `spdlog` for high-performance logging.
- **Configuration**: JSON configuration with SIGHUP hot-reload.

## Build & Packaging

### Quick Start (Oracle Linux / RHEL)
Run this single command to install dependencies, build the project, and generate the RPM:

```bash
sudo dnf install -y gcc-c++ cmake make git rpm-build && \
mkdir -p build && cd build && \
cmake .. && \
make package
```

### Manual Steps

1.  **Install Prerequisites**:
    ```bash
    sudo dnf install -y gcc-c++ cmake make git rpm-build
    ```

2.  **Configure & Build**:
    ```bash
    mkdir -p build && cd build
    cmake ..
    make package
    ```

3.  **Install RPM**:
    ```bash
    sudo rpm -ivh sentineld-1.0.0-Linux.rpm
    ```

4.  **Start Service**:
    ```bash
    sudo systemctl enable --now sentineld
    ```

## Configuration
Configuration is located at `/etc/sentineld/sentinel.json` (default).

```json
{
  "daemon": {
    "pid_file": "/run/sentineld/sentineld.pid",
    "log_file": "/var/log/sentineld/sentineld.log",
    "log_level": "info"
  },
  "monitor": {
    "interval_seconds": 5,
    "cpu_threshold_percent": 90,
    "memory_threshold_percent": 85
  },
  "watcher": {
    "path": "/etc/sentineld",
    "events": ["create", "modify", "delete", "move"]
  }
}
```

Supported `watcher.events`: `create`, `delete`, `modify`, `close_write`, `move`, `attrib`.

Reload without restarting:

```bash
sudo systemctl reload sentineld
```

Note that `log_file` is only read at startup; changing it requires a restart.

## Known limitations

- The watch is not recursive; subdirectories of `watcher.path` are not monitored.
- This is change *detection*, not file *integrity* monitoring: no baseline hashes are kept.
- `/proc` sampling reports host-wide figures and is not cgroup-aware, so numbers are wrong inside a container.
- There are no automated tests yet.


# sentineld - Linux System Monitor Daemon

`sentineld` is a production-quality C++ daemon designed for Linux systems. It monitors system resources (CPU, Memory) and file integrity (using inotify).

## Features

- **Daemonization**: Runs as a background process with PID file management.
- **Signal Handling**: Graceful shutdown (SIGTERM/SIGINT) and configuration reload (SIGHUP).
- **Resource Monitoring**: Tracks CPU and Memory usage via `/proc`, alerting on high load.
- **File Integrity Monitoring**: Watches critical directories for file creation, modification, and deletion events.
- **Structured Logging**: Uses `spdlog` for high-performance logging.
- **Configuration**: YAML-based configuration with hot-reload support.

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
    "pid_file": "/var/run/sentineld.pid",
    "log_file": "/var/log/sentineld.log",
    "log_level": "info"
  },
  "monitor": {
    "interval_seconds": 5,
    "cpu_threshold_percent": 90,
    "memory_threshold_percent": 85
  },
  "watcher": {
    "path": "/tmp/sentinel_watch",
    "events": ["create", "modify", "delete"]
  }
}
```

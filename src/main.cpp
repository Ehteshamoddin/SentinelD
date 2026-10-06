#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "sentinel/core/config.hpp"
#include "sentinel/core/logger.hpp"
#include "sentinel/daemon/daemonizer.hpp"
#include "sentinel/daemon/signal_handler.hpp"
#include "sentinel/features/file_watcher.hpp"
#include "sentinel/features/resource_monitor.hpp"

using namespace sentinel::core;
namespace daemon_ns = sentinel::daemon;

int main(int argc, char** argv) {
    CLI::App app{"sentineld - Linux System Monitor Daemon"};

    std::string config_path = "/etc/sentineld/sentinel.json";
    bool daemon_mode = false;
    app.add_option("-c,--config", config_path, "Path to configuration file");
    app.add_flag("-d,--daemon", daemon_mode, "Run as a background daemon");
    app.set_version_flag("-v,--version", std::string(SENTINEL_VERSION));

    CLI11_PARSE(app, argc, argv);

    // Console-only until we know where the real log should go.
    Logger::InitConsole();
    spdlog::info("Starting sentineld {}...", SENTINEL_VERSION);

    // Fail fast rather than running on a half-known configuration: a daemon
    // that starts with bad settings is harder to debug than one that refuses.
    if (!Config::GetInstance().Load(config_path)) {
        spdlog::critical("Could not load configuration from {}; refusing to start", config_path);
        return 1;
    }

    auto config = Config::GetInstance().GetData();

    // Daemonize BEFORE opening the log file. Daemonize() closes every inherited
    // descriptor, which would otherwise close the file sink's fd underneath
    // spdlog and silently discard all file logging.
    if (daemon_mode) {
        spdlog::info("Switching to daemon mode...");
        daemon_ns::Daemonizer::Daemonize();
    }

    Logger::Init(config.daemon.log_level, config.daemon.log_file);
    spdlog::info("Configuration loaded. Log level: {}", config.daemon.log_level);
    if (!config.daemon.log_file.empty()) {
        spdlog::info("Logging to file: {}", config.daemon.log_file);
    }

    // Single-instance guard. Held for the process lifetime; released below.
    if (!daemon_ns::Daemonizer::AcquirePidFile(config.daemon.pid_file)) {
        return 1;
    }

    if (!daemon_ns::SignalHandler::Init()) {
        daemon_ns::Daemonizer::ReleasePidFile();
        return 1;
    }

    sentinel::features::ResourceMonitor resource_monitor;
    sentinel::features::FileWatcher file_watcher;

    file_watcher.Start(config.watcher.path, config.watcher.events);

    spdlog::info("Entering main loop (interval {}s)...", config.monitor.interval_seconds);
    while (!daemon_ns::SignalHandler::ShouldExit()) {
        if (daemon_ns::SignalHandler::ShouldReload()) {
            // Cleared first, so a SIGHUP arriving during the reload is handled
            // on the next iteration rather than being swallowed.
            daemon_ns::SignalHandler::ResetReload();
            spdlog::info("Reloading configuration...");

            if (Config::GetInstance().Load(config_path)) {
                // This assignment was missing: the loop kept using the interval
                // captured before the loop started.
                config = Config::GetInstance().GetData();

                // Only the level is changed at runtime. Rebuilding the logger
                // would call set_default_logger() while the watcher thread is
                // logging, which is a data race. Changing log_file therefore
                // requires a restart.
                Logger::SetLevel(config.daemon.log_level);

                file_watcher.Stop();
                file_watcher.Start(config.watcher.path, config.watcher.events);
                spdlog::info("Reload complete (interval {}s)", config.monitor.interval_seconds);
            } else {
                spdlog::error("Reload failed; keeping the previous configuration");
            }
        }

        resource_monitor.Check();

        // Interruptible: returns as soon as a signal arrives, instead of always
        // sleeping the full interval and delaying shutdown by up to that long.
        daemon_ns::SignalHandler::Wait(config.monitor.interval_seconds * 1000);
    }

    spdlog::info("Shutting down gracefully...");
    file_watcher.Stop();
    daemon_ns::Daemonizer::ReleasePidFile();
    spdlog::info("sentineld stopped");
    return 0;
}

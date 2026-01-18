#include <iostream>
#include <CLI/CLI.hpp>
#include "sentinel/core/config.hpp"
#include "sentinel/core/logger.hpp"
#include "sentinel/daemon/daemonizer.hpp"
#include "sentinel/daemon/signal_handler.hpp"
#include "sentinel/features/resource_monitor.hpp"
#include "sentinel/features/file_watcher.hpp"
#include <thread>
#include <chrono>

using namespace sentinel::core;

int main(int argc, char** argv) {
    CLI::App app{"sentineld - Linux System Monitor Daemon"};
    
    std::string config_path = "/etc/sentineld/sentinel.json";
    bool daemon_mode = false;
    app.add_option("-c,--config", config_path, "Path to configuration file");
    app.add_flag("-d,--daemon", daemon_mode, "Run as a background daemon");
    app.set_version_flag("-v,--version", std::string(SENTINEL_VERSION));

    CLI11_PARSE(app, argc, argv);

    // Initialize Logger
    Logger::Init("info");
    spdlog::info("Starting sentineld...");

    // Load Configuration
    if (!Config::GetInstance().Load(config_path)) {
        spdlog::warn("Failed to load config from {}, using defaults", config_path);
    }

    auto config = Config::GetInstance().GetData();
    
    // Re-configure logger
    Logger::Init(config.daemon.log_level, config.daemon.log_file);
    spdlog::info("Configuration loaded. Log level: {}", config.daemon.log_level);
    if (!config.daemon.log_file.empty()) {
        spdlog::info("Logging to file: {}", config.daemon.log_file);
    }

    // Daemonize if requested
    if (daemon_mode) {
        spdlog::info("Switching to daemon mode...");
        sentinel::daemon::Daemonizer::Daemonize(config.daemon.pid_file);
    }
    
    // Setup Signal Handling
    sentinel::daemon::SignalHandler::Init();

    // Initialize Features
    sentinel::features::ResourceMonitor resource_monitor;
    sentinel::features::FileWatcher file_watcher;

    // Start File Watcher
    // Ensure the directory exists or handle error inside Start
    file_watcher.Start(config.watcher.path, config.watcher.events);

    // Main Loop
    spdlog::info("Entering main loop...");
    while (!sentinel::daemon::SignalHandler::ShouldExit()) {
        if (sentinel::daemon::SignalHandler::ShouldReload()) {
            spdlog::info("Reloading configuration...");
            if (Config::GetInstance().Load(config_path)) {
                auto new_config = Config::GetInstance().GetData();
                Logger::Init(new_config.daemon.log_level);
                
                // Restart watcher if path changed (simplified: just restart)
                file_watcher.Stop();
                file_watcher.Start(new_config.watcher.path, new_config.watcher.events);
            }
            sentinel::daemon::SignalHandler::ResetReload();
        }

        // Run Features
        resource_monitor.Check();
        
        // Sleep to prevent CPU spinning (replace with event loop later)
        std::this_thread::sleep_for(std::chrono::seconds(config.monitor.interval_seconds));
    }

    spdlog::info("Shutting down gracefully...");
    file_watcher.Stop();
    return 0;
}

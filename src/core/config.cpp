#include "sentinel/core/config.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

namespace sentinel::core {

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

bool Config::Load(const std::string& path) {
    ConfigData candidate;
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open config file: {}", path);
            return false;
        }

        nlohmann::json j;
        file >> j;
        candidate = j.get<ConfigData>();
    } catch (const std::exception& e) {
        spdlog::error("Error parsing config file {}: {}", path, e.what());
        return false;
    }

    std::string error;
    if (!Validate(candidate, error)) {
        spdlog::error("Rejecting config from {}: {}", path, error);
        return false;
    }

    // Only publish once the candidate is fully built and validated, so a
    // failed load can never leave a half-applied configuration behind.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        data_ = std::move(candidate);
    }
    spdlog::info("Configuration loaded successfully from {}", path);
    return true;
}

ConfigData Config::GetData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

bool Validate(const ConfigData& c, std::string& error) {
    if (c.monitor.interval_seconds <= 0) {
        error = "monitor.interval_seconds must be greater than 0";
        return false;
    }
    if (c.monitor.cpu_threshold_percent <= 0 || c.monitor.cpu_threshold_percent > 100) {
        error = "monitor.cpu_threshold_percent must be in (0, 100]";
        return false;
    }
    if (c.monitor.memory_threshold_percent <= 0 || c.monitor.memory_threshold_percent > 100) {
        error = "monitor.memory_threshold_percent must be in (0, 100]";
        return false;
    }
    if (c.daemon.pid_file.empty()) {
        error = "daemon.pid_file must not be empty";
        return false;
    }
    return true;
}

// Every field uses value() against the struct's own default, so a config that
// omits a key is accepted rather than aborting the whole parse.
void from_json(const nlohmann::json& j, ConfigData::Daemon& d) {
    d.pid_file  = j.value("pid_file", d.pid_file);
    d.log_file  = j.value("log_file", d.log_file);
    d.log_level = j.value("log_level", d.log_level);
}

void from_json(const nlohmann::json& j, ConfigData::Monitor& m) {
    m.interval_seconds         = j.value("interval_seconds", m.interval_seconds);
    m.cpu_threshold_percent    = j.value("cpu_threshold_percent", m.cpu_threshold_percent);
    m.memory_threshold_percent = j.value("memory_threshold_percent", m.memory_threshold_percent);
}

void from_json(const nlohmann::json& j, ConfigData::Watcher& w) {
    w.path   = j.value("path", w.path);
    w.events = j.value("events", w.events);
}

void from_json(const nlohmann::json& j, ConfigData& c) {
    if (j.contains("daemon"))  j.at("daemon").get_to(c.daemon);
    if (j.contains("monitor")) j.at("monitor").get_to(c.monitor);
    if (j.contains("watcher")) j.at("watcher").get_to(c.watcher);
}

} // namespace sentinel::core

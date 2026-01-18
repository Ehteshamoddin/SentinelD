#include "sentinel/core/config.hpp"
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>

namespace sentinel::core {

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

bool Config::Load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open config file: {}", path);
            return false;
        }

        nlohmann::json j;
        file >> j;
        data_ = j.get<ConfigData>();
        spdlog::info("Configuration loaded successfully from {}", path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing config file: {}", e.what());
        return false;
    }
}

ConfigData Config::GetData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

void from_json(const nlohmann::json& j, ConfigData::Daemon& d) {
    j.at("pid_file").get_to(d.pid_file);
    // Optional: use value() to provide default if missing, or at() to enforce
    if (j.contains("log_file")) {
        j.at("log_file").get_to(d.log_file);
    }
    j.at("log_level").get_to(d.log_level);
}

void from_json(const nlohmann::json& j, ConfigData::Monitor& m) {
    j.at("interval_seconds").get_to(m.interval_seconds);
    j.at("cpu_threshold_percent").get_to(m.cpu_threshold_percent);
    j.at("memory_threshold_percent").get_to(m.memory_threshold_percent);
}

void from_json(const nlohmann::json& j, ConfigData::Watcher& w) {
    j.at("path").get_to(w.path);
    j.at("events").get_to(w.events);
}

void from_json(const nlohmann::json& j, ConfigData& c) {
    j.at("daemon").get_to(c.daemon);
    j.at("monitor").get_to(c.monitor);
    j.at("watcher").get_to(c.watcher);
}

} // namespace sentinel::core

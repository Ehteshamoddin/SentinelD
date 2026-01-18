#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <mutex>

namespace sentinel::core {

struct ConfigData {
    struct Daemon {
        std::string pid_file;
        std::string log_file;
        std::string log_level;
    } daemon;

    struct Monitor {
        int interval_seconds;
        int cpu_threshold_percent;
        int memory_threshold_percent;
    } monitor;

    struct Watcher {
        std::string path;
        std::vector<std::string> events;
    } watcher;
};

class Config {
public:
    static Config& GetInstance();

    bool Load(const std::string& path);
    ConfigData GetData() const;

private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    mutable std::mutex mutex_;
    ConfigData data_;
};

// JSON serialization support
void from_json(const nlohmann::json& j, ConfigData::Daemon& d);
void from_json(const nlohmann::json& j, ConfigData::Monitor& m);
void from_json(const nlohmann::json& j, ConfigData::Watcher& w);
void from_json(const nlohmann::json& j, ConfigData& c);

} // namespace sentinel::core

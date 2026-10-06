#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

namespace sentinel::core {

// Every field carries a default so that a partial or absent configuration
// still yields a daemon that runs sanely rather than one that spins.
struct ConfigData {
    struct Daemon {
        std::string pid_file{"/var/run/sentineld.pid"};
        std::string log_file{};
        std::string log_level{"info"};
    } daemon;

    struct Monitor {
        int interval_seconds{5};
        int cpu_threshold_percent{90};
        int memory_threshold_percent{85};
    } monitor;

    struct Watcher {
        std::string path{};
        std::vector<std::string> events{"create", "modify", "delete"};
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

// Rejects a candidate that would make the daemon misbehave (zero interval,
// out-of-range thresholds). Returns false and fills `error` on failure.
bool Validate(const ConfigData& c, std::string& error);

// JSON serialization support
void from_json(const nlohmann::json& j, ConfigData::Daemon& d);
void from_json(const nlohmann::json& j, ConfigData::Monitor& m);
void from_json(const nlohmann::json& j, ConfigData::Watcher& w);
void from_json(const nlohmann::json& j, ConfigData& c);

} // namespace sentinel::core

#include "sentinel/features/resource_monitor.hpp"
#include "sentinel/core/config.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <string>

namespace sentinel::features {

ResourceMonitor::ResourceMonitor() {
    // Initialize previous stats
    try {
        prev_stats_ = ReadCpuStats();
    } catch (...) {
        first_run_ = true;
    }
}

void ResourceMonitor::Check() {
    auto config = core::Config::GetInstance().GetData().monitor;

    // CPU Check
    try {
        CpuStats curr_stats = ReadCpuStats();
        if (!first_run_) {
            double cpu_usage = CalculateCpuUsage(prev_stats_, curr_stats);
            spdlog::debug("CPU Usage: {:.2f}%", cpu_usage);

            if (cpu_usage > config.cpu_threshold_percent) {
                spdlog::warn("High CPU Usage detected: {:.2f}% (Threshold: {}%)", 
                             cpu_usage, config.cpu_threshold_percent);
            }
        }
        prev_stats_ = curr_stats;
        first_run_ = false;
    } catch (const std::exception& e) {
        spdlog::error("Failed to check CPU usage: {}", e.what());
    }

    // Memory Check
    try {
        double mem_usage = GetMemoryUsage();
        spdlog::debug("Memory Usage: {:.2f}%", mem_usage);

        if (mem_usage > config.memory_threshold_percent) {
            spdlog::warn("High Memory Usage detected: {:.2f}% (Threshold: {}%)", 
                         mem_usage, config.memory_threshold_percent);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to check Memory usage: {}", e.what());
    }
}

ResourceMonitor::CpuStats ResourceMonitor::ReadCpuStats() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open /proc/stat");
    }

    std::string line;
    std::getline(file, line); // First line is total cpu
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label;

    CpuStats stats;
    iss >> stats.user >> stats.nice >> stats.system >> stats.idle 
        >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
    
    return stats;
}

double ResourceMonitor::CalculateCpuUsage(const CpuStats& prev, const CpuStats& curr) {
    unsigned long long prev_idle = prev.idle + prev.iowait;
    unsigned long long curr_idle = curr.idle + curr.iowait;

    unsigned long long prev_non_idle = prev.user + prev.nice + prev.system + prev.irq + prev.softirq + prev.steal;
    unsigned long long curr_non_idle = curr.user + curr.nice + curr.system + curr.irq + curr.softirq + curr.steal;

    unsigned long long prev_total = prev_idle + prev_non_idle;
    unsigned long long curr_total = curr_idle + curr_non_idle;

    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;

    if (total_diff == 0) return 0.0;

    return (double)(total_diff - idle_diff) / total_diff * 100.0;
}

double ResourceMonitor::GetMemoryUsage() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open /proc/meminfo");
    }

    unsigned long long total_mem = 0;
    unsigned long long free_mem = 0;
    unsigned long long available_mem = 0; // Better metric if available

    std::string key;
    unsigned long long value;
    std::string unit;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") total_mem = value;
        else if (key == "MemFree:") free_mem = value;
        else if (key == "MemAvailable:") available_mem = value;
    }

    if (total_mem == 0) return 0.0;

    // Use MemAvailable if present (kernels > 3.14), otherwise fall back to MemFree
    unsigned long long used_mem = total_mem - (available_mem > 0 ? available_mem : free_mem);
    
    return (double)used_mem / total_mem * 100.0;
}

} // namespace sentinel::features

#include "sentinel/features/resource_monitor.hpp"
#include "sentinel/core/config.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace sentinel::features {

ResourceMonitor::ResourceMonitor() {
    try {
        prev_stats_ = ReadCpuStats();
        // Previously this was left true, which discarded the sample we just
        // took and made the constructor's read pointless.
        first_run_ = false;
    } catch (const std::exception& e) {
        spdlog::warn("Initial CPU sample failed ({}); first check will be skipped", e.what());
        first_run_ = true;
    }
}

void ResourceMonitor::Check() {
    auto config = core::Config::GetInstance().GetData().monitor;

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
    if (!std::getline(file, line)) {
        throw std::runtime_error("/proc/stat is empty");
    }

    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label;
    if (cpu_label != "cpu") {
        throw std::runtime_error("Unexpected first line in /proc/stat");
    }

    CpuStats stats{};
    iss >> stats.user >> stats.nice >> stats.system >> stats.idle
        >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
    if (iss.fail()) {
        throw std::runtime_error("Malformed aggregate cpu line in /proc/stat");
    }

    return stats;
}

double ResourceMonitor::CalculateCpuUsage(const CpuStats& prev, const CpuStats& curr) {
    unsigned long long prev_idle = prev.idle + prev.iowait;
    unsigned long long curr_idle = curr.idle + curr.iowait;

    // guest and guest_nice are deliberately not summed: the kernel already
    // counts them inside user and nice.
    unsigned long long prev_non_idle =
        prev.user + prev.nice + prev.system + prev.irq + prev.softirq + prev.steal;
    unsigned long long curr_non_idle =
        curr.user + curr.nice + curr.system + curr.irq + curr.softirq + curr.steal;

    unsigned long long prev_total = prev_idle + prev_non_idle;
    unsigned long long curr_total = curr_idle + curr_non_idle;

    // Counters are monotonic in normal operation, but CPU hotplug or suspend
    // can move them backwards; unsigned subtraction would wrap to a huge value.
    if (curr_total <= prev_total || curr_idle < prev_idle) {
        return 0.0;
    }

    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;

    return static_cast<double>(total_diff - idle_diff) / static_cast<double>(total_diff) * 100.0;
}

double ResourceMonitor::GetMemoryUsage() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open /proc/meminfo");
    }

    unsigned long long total_mem = 0;
    unsigned long long free_mem = 0;
    unsigned long long available_mem = 0;

    // Parsed line by line. A bare `file >> key >> value >> unit` desynchronises
    // on the two-token lines (HugePages_Total: 0) further down the file.
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        unsigned long long value = 0;
        if (!(iss >> key >> value)) {
            continue;
        }

        if (key == "MemTotal:")          total_mem = value;
        else if (key == "MemFree:")      free_mem = value;
        else if (key == "MemAvailable:") available_mem = value;

        if (total_mem != 0 && available_mem != 0) {
            break; // everything we need is in the first three lines
        }
    }

    if (total_mem == 0) {
        throw std::runtime_error("MemTotal not found in /proc/meminfo");
    }

    // MemAvailable (kernel >= 3.14) accounts for reclaimable page cache;
    // MemFree alone reads as near-zero on a healthy machine.
    unsigned long long usable = (available_mem > 0) ? available_mem : free_mem;
    if (usable > total_mem) {
        usable = total_mem;
    }

    return static_cast<double>(total_mem - usable) / static_cast<double>(total_mem) * 100.0;
}

} // namespace sentinel::features

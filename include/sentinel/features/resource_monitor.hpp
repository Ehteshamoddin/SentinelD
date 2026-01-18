#pragma once

namespace sentinel::features {

class ResourceMonitor {
public:
    ResourceMonitor();
    void Check();

private:
    struct CpuStats {
        unsigned long long user;
        unsigned long long nice;
        unsigned long long system;
        unsigned long long idle;
        unsigned long long iowait;
        unsigned long long irq;
        unsigned long long softirq;
        unsigned long long steal;
    };

    CpuStats ReadCpuStats();
    double CalculateCpuUsage(const CpuStats& prev, const CpuStats& curr);
    double GetMemoryUsage();

    CpuStats prev_stats_;
    bool first_run_{true};
};

} // namespace sentinel::features

#pragma once

#include "ResourceSampler.hpp"

#include <string_view>

namespace metrics::detail {

template<class T> Metric<T> available(T value)
{
    return {value, SampleStatus::Ok};
}

template<class T> Metric<T> missing(SampleStatus status = SampleStatus::Unavailable)
{
    return {std::nullopt, status};
}

struct SystemCpuTimes {
    std::uint64_t idle;
    std::uint64_t kernel; // Includes idle, as returned by GetSystemTimes.
    std::uint64_t user;
};

class SystemCpuDelta {
public:
    Metric<double> sample(std::optional<SystemCpuTimes> current);
    void reset() { previous_.reset(); }
private:
    std::optional<SystemCpuTimes> previous_;
};

struct ProcessCpuTimes {
    std::uint64_t kernel100ns;
    std::uint64_t user100ns;
    std::uint64_t wallTicks; // Monotonic QPC, not UTC wall time.
    std::uint64_t wallFrequency;
    std::uint32_t logicalProcessors; // All active processor groups.
};

class ProcessCpuDelta {
public:
    Metric<double> sample(std::optional<ProcessCpuTimes> current);
    void reset() { previous_.reset(); }
private:
    std::optional<ProcessCpuTimes> previous_;
};

Metric<std::uint64_t> physicalMemoryUsed(std::uint64_t total, std::uint64_t available);

struct GpuInstance {
    std::uint64_t luid;
    std::uint32_t physical;
    std::uint32_t engine = 0;
    std::uint32_t process = 0;
};

std::optional<GpuInstance> parseEngineInstance(std::wstring_view name);
std::optional<GpuInstance> parseMemoryInstance(std::wstring_view name);

struct EngineCounter {
    GpuInstance instance;
    Metric<double> percent;
};

struct MemoryCounter {
    GpuInstance instance;
    Metric<std::uint64_t> bytes;
};

// Sum processes sharing the same physical engine, then take the busiest engine.
// Duplicate process/engine aliases count once. Invalid matching rows make the
// aggregate unavailable instead of silently reporting a misleading partial sum.
Metric<double> busiestEngine(std::uint64_t luid, const std::vector<EngineCounter> &counters);
Metric<std::uint64_t> dedicatedMemory(std::uint64_t luid, const std::vector<MemoryCounter> &counters);

} // namespace metrics::detail

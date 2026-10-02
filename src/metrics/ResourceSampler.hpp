#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace metrics {

enum class SampleStatus { Ok, WarmingUp, Unavailable, Unsupported };

// A missing value is never a measured zero. Only Ok carries a value.
template<class T> struct Metric {
    std::optional<T> value;
    SampleStatus status = SampleStatus::Unavailable;
};

struct GpuSnapshot {
    std::string id;   // Adapter LUID, stable for this Windows session.
    std::string name; // UTF-8, from DXGI.
    Metric<double> utilizationPercent; // Busiest physical engine, all processes.
    Metric<std::uint64_t> dedicatedUsedBytes; // Adapter-wide, not process usage.
    Metric<std::uint64_t> dedicatedTotalBytes;
};

struct ResourceSnapshot {
    Metric<double> systemCpuPercent;
    Metric<double> processCpuPercent; // Current process / all active logical CPUs.
    Metric<std::uint64_t> systemMemoryUsedBytes;
    Metric<std::uint64_t> systemMemoryTotalBytes;
    Metric<std::uint64_t> processWorkingSetBytes;
    std::vector<GpuSnapshot> gpus;
    // Adapter discovery state. Individual counters have their own status.
    SampleStatus gpuStatus = SampleStatus::Unavailable;
};

// Construct, sample, reset and destroy on one worker thread. OS calls can block;
// do not call from the GUI thread. Recommended cadence: one sample per second.
// OS failures are represented in the snapshot; allocation failures may throw.
class ResourceSampler {
public:
    ResourceSampler();
    ~ResourceSampler();
    ResourceSampler(const ResourceSampler &) = delete;
    ResourceSampler &operator=(const ResourceSampler &) = delete;

    ResourceSnapshot sample();
    void reset(); // Drops rate baselines and retries adapter/counter discovery.

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace metrics

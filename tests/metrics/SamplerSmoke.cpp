#include "src/metrics/ResourceSampler.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace metrics;

namespace {
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template<class T> void invariant(const Metric<T> &metric)
{
    require(metric.value.has_value() == (metric.status == SampleStatus::Ok), "metric/status mismatch");
}

void percentage(const Metric<double> &metric)
{
    invariant(metric);
    if (metric.value)
        require(std::isfinite(*metric.value) && *metric.value >= 0 && *metric.value <= 100,
                "percentage out of range");
}

void validate(const ResourceSnapshot &snapshot)
{
    percentage(snapshot.systemCpuPercent);
    percentage(snapshot.processCpuPercent);
    invariant(snapshot.systemMemoryTotalBytes);
    invariant(snapshot.systemMemoryUsedBytes);
    invariant(snapshot.processWorkingSetBytes);
    require(snapshot.systemMemoryTotalBytes.value && snapshot.systemMemoryUsedBytes.value &&
            snapshot.processWorkingSetBytes.value, "required RAM metrics unavailable");
    require(*snapshot.systemMemoryTotalBytes.value > 0 &&
            *snapshot.systemMemoryUsedBytes.value <= *snapshot.systemMemoryTotalBytes.value &&
            *snapshot.processWorkingSetBytes.value > 0, "RAM metric invalid");
    for (const auto &gpu : snapshot.gpus) {
        require(!gpu.id.empty() && !gpu.name.empty(), "adapter identity empty");
        percentage(gpu.utilizationPercent);
        invariant(gpu.dedicatedUsedBytes);
        invariant(gpu.dedicatedTotalBytes);
        if (gpu.dedicatedTotalBytes.value)
            require(*gpu.dedicatedTotalBytes.value > 0, "VRAM denominator is zero");
    }
}

const char *statusName(SampleStatus status)
{
    switch (status) {
    case SampleStatus::Ok: return "ok";
    case SampleStatus::WarmingUp: return "warming-up";
    case SampleStatus::Unavailable: return "unavailable";
    case SampleStatus::Unsupported: return "unsupported";
    }
    return "invalid";
}
} // namespace

int main(int argc, char **argv)
{
    try {
        int samples = 4;
        bool requireGpu = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--require-gpu") {
                requireGpu = true;
            } else if (std::string(argv[i]) == "--samples" && i + 1 < argc) {
                samples = std::stoi(argv[++i]);
                require(samples >= 4 && samples <= 60, "sample count must be between 4 and 60");
            } else {
                throw std::runtime_error("usage: SamplerSmoke [--require-gpu] [--samples 4..60]");
            }
        }
        ResourceSampler sampler;
        bool cpuAvailable = false;
        bool gpuAvailable = false;
        for (int iteration = 0; iteration < samples; ++iteration) {
            if (iteration)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            const auto started = std::chrono::steady_clock::now();
            const auto snapshot = sampler.sample();
            const auto elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            validate(snapshot);
            if (iteration == 0) {
                require(!snapshot.processCpuPercent.value &&
                        snapshot.processCpuPercent.status == SampleStatus::WarmingUp,
                        "first process CPU sample must warm up");
                require(!snapshot.systemCpuPercent.value, "first system CPU sample must not be zero");
            } else {
                cpuAvailable = cpuAvailable || (snapshot.systemCpuPercent.value && snapshot.processCpuPercent.value);
                bool allGpus = !snapshot.gpus.empty();
                for (const auto &gpu : snapshot.gpus)
                    allGpus = allGpus && gpu.utilizationPercent.value && gpu.dedicatedUsedBytes.value &&
                              gpu.dedicatedTotalBytes.value;
                gpuAvailable = gpuAvailable || allGpus;
            }
            std::cout << "sample=" << iteration << " elapsed_ms=" << elapsed
                      << " system_cpu=" << statusName(snapshot.systemCpuPercent.status)
                      << " process_cpu=" << statusName(snapshot.processCpuPercent.status)
                      << " adapters=" << snapshot.gpus.size() << '\n';
            // Report status/numeric values without copying hardware identity into CI logs.
            for (std::size_t index = 0; index < snapshot.gpus.size(); ++index) {
                const auto &gpu = snapshot.gpus[index];
                std::cout << "gpu=" << index << " usage=" << statusName(gpu.utilizationPercent.status);
                if (gpu.utilizationPercent.value)
                    std::cout << ':' << *gpu.utilizationPercent.value;
                std::cout << " dedicated_used=" << statusName(gpu.dedicatedUsedBytes.status)
                          << " dedicated_total=" << statusName(gpu.dedicatedTotalBytes.status) << '\n';
            }
        }
        require(cpuAvailable, "CPU metrics did not become available after warmup");
        require(!requireGpu || gpuAvailable, "not all adapters exposed GPU usage and dedicated memory");

        // Exercise a real CPU-time increment in this process without assuming
        // this CI host can dedicate a full core or a fixed number of cores.
        const auto busyUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now() < busyUntil)
            std::atomic_signal_fence(std::memory_order_seq_cst);
        const auto busy = sampler.sample();
        validate(busy);
        require(busy.processCpuPercent.value && *busy.processCpuPercent.value > 0,
                "busy current process did not report CPU activity");

        sampler.reset();
        const auto restarted = sampler.sample();
        validate(restarted);
        require(!restarted.processCpuPercent.value &&
                restarted.processCpuPercent.status == SampleStatus::WarmingUp,
                "reset must drop process baseline");
        require(!restarted.systemCpuPercent.value, "reset must drop system baseline");

        // Warm platform caches before comparing handle counts. PDH/DXGI may
        // retain a small process-wide cache, but queries must not leak per owner.
        {
            ResourceSampler warm;
            validate(warm.sample());
        }
        DWORD before = 0, after = 0;
        require(GetProcessHandleCount(GetCurrentProcess(), &before) != FALSE, "handle baseline unavailable");
        for (int i = 0; i < 6; ++i) {
            ResourceSampler temporary;
            validate(temporary.sample());
        }
        require(GetProcessHandleCount(GetCurrentProcess(), &after) != FALSE, "handle readback unavailable");
        require(after <= before + 4, "sampler handles grew after repeated destruction");
        std::cout << "lifecycle_handles_before=" << before << " after=" << after << '\n';
        std::cout << "SamplerSmoke passed; GPU availability is reported, not required on headless CI.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "SamplerSmoke failed: " << error.what() << '\n';
        return 1;
    }
}

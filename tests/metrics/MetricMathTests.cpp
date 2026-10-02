#include "src/metrics/MetricMath.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace metrics;
using namespace metrics::detail;

namespace {
int checks = 0;

void require(bool condition, const char *message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

template<class T> void absent(const Metric<T> &metric, SampleStatus status)
{
    require(!metric.value && metric.status == status, "missing metric/status mismatch");
}

void percent(const Metric<double> &metric, double expected)
{
    require(metric.status == SampleStatus::Ok && metric.value &&
            std::abs(*metric.value - expected) < 0.000001, "unexpected CPU/GPU percentage");
}

void systemCpuTests()
{
    SystemCpuDelta cpu;
    absent(cpu.sample(SystemCpuTimes{100, 150, 50}), SampleStatus::WarmingUp);
    percent(cpu.sample(SystemCpuTimes{160, 270, 130}), 70); // Kernel already includes idle.
    percent(cpu.sample(SystemCpuTimes{260, 370, 130}), 0); // Real idle is available.
    absent(cpu.sample(SystemCpuTimes{260, 370, 130}), SampleStatus::Unavailable);
    absent(cpu.sample(SystemCpuTimes{0, 0, 0}), SampleStatus::WarmingUp); // Reset, no unsigned wrap.
    percent(cpu.sample(SystemCpuTimes{0, 50, 50}), 100);
    absent(cpu.sample(SystemCpuTimes{500, 51, 51}), SampleStatus::Unavailable);
    cpu.reset();
    absent(cpu.sample(SystemCpuTimes{0, 0, 0}), SampleStatus::WarmingUp);
    absent(cpu.sample(SystemCpuTimes{50, 40, 100}), SampleStatus::Unavailable); // Idle must be in kernel.
    absent(cpu.sample(std::nullopt), SampleStatus::Unavailable);
    absent(cpu.sample(SystemCpuTimes{1000, 2000, 1000}), SampleStatus::WarmingUp);
    cpu.reset();
    absent(cpu.sample(SystemCpuTimes{1001, 2001, 1001}), SampleStatus::WarmingUp);
    cpu.reset();
    absent(cpu.sample(SystemCpuTimes{0, 0, 0}), SampleStatus::WarmingUp);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    percent(cpu.sample(SystemCpuTimes{maximum, maximum, maximum}), 50); // Sum exceeds uint64_t.
}

void processCpuTests()
{
    ProcessCpuDelta cpu;
    absent(cpu.sample(ProcessCpuTimes{0, 0, 1000, 1000, 4}), SampleStatus::WarmingUp);
    percent(cpu.sample(ProcessCpuTimes{5000000, 5000000, 2000, 1000, 4}), 25);
    percent(cpu.sample(ProcessCpuTimes{5000000, 5000000, 3000, 1000, 4}), 0);
    absent(cpu.sample(ProcessCpuTimes{6000000, 5000000, 3000, 1000, 4}), SampleStatus::Unavailable);
    percent(cpu.sample(ProcessCpuTimes{10000000, 10000000, 4000, 1000, 4}), 25);
    absent(cpu.sample(ProcessCpuTimes{10000000, 10000000, 5000, 1000, 8}), SampleStatus::WarmingUp);
    percent(cpu.sample(ProcessCpuTimes{15000000, 15000000, 6000, 1000, 8}), 12.5);
    absent(cpu.sample(ProcessCpuTimes{1, 1, 7000, 1000, 8}), SampleStatus::WarmingUp);
    absent(cpu.sample(ProcessCpuTimes{2, 2, 1, 1000, 8}), SampleStatus::WarmingUp);
    absent(cpu.sample(ProcessCpuTimes{3, 3, 2, 2000, 8}), SampleStatus::WarmingUp);
    absent(cpu.sample(ProcessCpuTimes{4, 4, 3, 0, 8}), SampleStatus::Unavailable);
    absent(cpu.sample(ProcessCpuTimes{5, 5, 4, 2000, 0}), SampleStatus::Unavailable);
    absent(cpu.sample(ProcessCpuTimes{6, 6, 5, 2000, 8}), SampleStatus::WarmingUp);
    absent(cpu.sample(std::nullopt), SampleStatus::Unavailable);
    absent(cpu.sample(ProcessCpuTimes{7, 7, 6, 2000, 8}), SampleStatus::WarmingUp);
    cpu.reset();
    absent(cpu.sample(ProcessCpuTimes{0, 0, 0, 10000000, 256}), SampleStatus::WarmingUp);
    percent(cpu.sample(ProcessCpuTimes{1000000000000ULL, 0, 1000000000000ULL, 10000000, 256}),
            100.0 / 256); // No wallTicks * coreCount integer overflow.
    cpu.reset();
    absent(cpu.sample(ProcessCpuTimes{0, 0, 0, 1000, 1}), SampleStatus::WarmingUp);
    percent(cpu.sample(ProcessCpuTimes{20000000, 0, 1000, 1000, 1}), 100); // Clamp scheduling jitter.
}

void memoryTests()
{
    auto metric = physicalMemoryUsed(100, 40);
    require(metric.value == 60 && metric.status == SampleStatus::Ok, "physical memory subtraction");
    metric = physicalMemoryUsed(100, 100);
    require(metric.value == 0 && metric.status == SampleStatus::Ok, "physical memory real zero");
    absent(physicalMemoryUsed(0, 0), SampleStatus::Unavailable);
    absent(physicalMemoryUsed(100, 101), SampleStatus::Unavailable);
    const auto max = std::numeric_limits<std::uint64_t>::max();
    require(physicalMemoryUsed(max, 0).value == max, "64-bit physical memory");
}

void parsingTests()
{
    auto engine = parseEngineInstance(L"pid_123_luid_0xABCDEF01_0x12345678_phys_2_eng_11_engtype_VideoDecode");
    require(engine && engine->luid == 0xABCDEF0112345678ULL && engine->physical == 2 &&
            engine->engine == 11 && engine->process == 123, "LUID/process/physical/engine parsing");
    require(parseEngineInstance(L"pid_0_luid_0x00000000_0x00000000_phys_0_eng_0_engtype_3D#1").has_value(),
            "duplicate alias parsing");
    for (const auto *invalid : {
             L"_Total", L"pid_123_luid_0x1_0x12345678_phys_0_eng_0_engtype_3D",
             L"pid_4294967296_luid_0x00000000_0x12345678_phys_0_eng_0_engtype_3D",
             L"pid_1_luid_0x0000000Z_0x12345678_phys_0_eng_0_engtype_3D",
             L"pid_1_luid_0x00000000_0x12345678_phys_-1_eng_0_engtype_3D",
             L"pid_1_luid_0x00000000_0x12345678_phys_0_eng_0_engtype_"})
        require(!parseEngineInstance(invalid), "malformed engine name accepted");
    auto memory = parseMemoryInstance(L"luid_0xABCDEF01_0x12345678_phys_2#1");
    require(memory && memory->luid == 0xABCDEF0112345678ULL && memory->physical == 2,
            "memory LUID/physical parsing");
    require(!parseMemoryInstance(L"luid_0x00000000_0x12345678_phys_0_extra"), "trailing memory name");
    require(!parseMemoryInstance(L"luid_0x00000000_0x12345678_phys_0#"), "empty duplicate suffix");
}

void engineTests()
{
    std::vector<EngineCounter> counters{
        {{1, 0, 0, 10}, available(30.0)}, {{1, 0, 0, 20}, available(50.0)},
        {{1, 0, 1, 10}, available(90.0)}, {{2, 0, 0, 10}, available(100.0)}};
    percent(busiestEngine(1, counters), 90); // Neither sums independent engines nor mixes GPUs.
    counters.push_back({{1, 0, 0, 20}, available(50.0)});
    percent(busiestEngine(1, counters), 90); // Duplicate is not another 50%.
    counters.push_back({{1, 1, 0, 10}, available(95.0)});
    percent(busiestEngine(1, counters), 95); // Same index on another physical GPU is distinct.
    counters.push_back({{1, 0, 0, 30}, available(40.0)});
    percent(busiestEngine(1, counters), 100);
    absent(busiestEngine(3, counters), SampleStatus::Unavailable); // Missing is not idle.
    counters.push_back({{1, 0, 2, 30}, missing<double>()});
    absent(busiestEngine(1, counters), SampleStatus::Unavailable);
    percent(busiestEngine(2, counters), 100); // Other adapter failure does not poison this one.
    percent(busiestEngine(1, {{{1, 0, 0, 1}, available(0.0)}}), 0);
    absent(busiestEngine(1, {{{1, 0, 0, 1}, available(-1.0)}}), SampleStatus::Unavailable);
    absent(busiestEngine(1, {{{1, 0, 0, 1}, available(std::numeric_limits<double>::quiet_NaN())}}),
           SampleStatus::Unavailable);
    absent(busiestEngine(1, {{{1, 0, 0, 1}, available(std::numeric_limits<double>::infinity())}}),
           SampleStatus::Unavailable);
    absent(busiestEngine(1, {{{1, 0, 0, 1}, missing<double>(SampleStatus::WarmingUp)}}),
           SampleStatus::WarmingUp);
}

void gpuMemoryTests()
{
    std::vector<MemoryCounter> counters{
        {{1, 0}, available<std::uint64_t>(100)}, {{1, 0}, available<std::uint64_t>(100)},
        {{1, 1}, available<std::uint64_t>(200)}, {{2, 0}, available<std::uint64_t>(400)}};
    require(dedicatedMemory(1, counters).value == 300, "adapter-wide memory/duplicate handling");
    require(dedicatedMemory(2, counters).value == 400, "VRAM adapter correlation");
    absent(dedicatedMemory(3, counters), SampleStatus::Unavailable);
    counters.push_back({{1, 2}, missing<std::uint64_t>()});
    absent(dedicatedMemory(1, counters), SampleStatus::Unavailable);
    require(dedicatedMemory(1, {{{1, 0}, available<std::uint64_t>(0)}}).value == 0, "real zero VRAM");
    absent(dedicatedMemory(1, {{{1, 0}, available(std::numeric_limits<std::uint64_t>::max())},
                              {{1, 1}, available<std::uint64_t>(1)}}), SampleStatus::Unavailable);
}
} // namespace

int main()
{
    try {
        systemCpuTests();
        processCpuTests();
        memoryTests();
        parsingTests();
        engineTests();
        gpuMemoryTests();
        std::cout << "MetricMath: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "MetricMath failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}

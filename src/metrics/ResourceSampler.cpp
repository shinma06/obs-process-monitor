#include "ResourceSampler.hpp"
#include "MetricMath.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dxgi1_2.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <utility>

namespace metrics {
namespace {

using detail::available;
using detail::missing;
using Clock = std::chrono::steady_clock;

constexpr DWORD maxCounterBytes = 4 * 1024 * 1024;
constexpr DWORD maxCounterItems = 16384;
constexpr UINT maxAdapters = 32;
constexpr auto retryInterval = std::chrono::seconds(5);
constexpr auto discoveryInterval = std::chrono::seconds(30);

std::uint64_t ticks(FILETIME time)
{
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

bool validCounter(DWORD status)
{
    return status == PDH_CSTATUS_VALID_DATA || status == PDH_CSTATUS_NEW_DATA;
}

SampleStatus counterFailure(PDH_STATUS status)
{
    const auto code = static_cast<DWORD>(status);
    return code == PDH_CSTATUS_NO_OBJECT || code == PDH_CSTATUS_NO_COUNTER ||
           code == PDH_FUNCTION_NOT_FOUND ? SampleStatus::Unsupported : SampleStatus::Unavailable;
}

template<class T> struct NamedCounter {
    std::wstring name;
    Metric<T> metric;
};

template<class T> struct CounterArray {
    SampleStatus status = SampleStatus::Unavailable;
    std::vector<NamedCounter<T>> items;
};

// One query per counter isolates a missing GPU provider from CPU sampling.
// Wildcard queries automatically discover newly created process instances.
class CounterQuery {
public:
    CounterQuery(const wchar_t *path, bool rate) : path_(path), rate_(rate) {}
    ~CounterQuery() { close(); }
    CounterQuery(const CounterQuery &) = delete;
    CounterQuery &operator=(const CounterQuery &) = delete;

    Metric<double> scalar()
    {
        const auto status = collect();
        if (status != SampleStatus::Ok)
            return missing<double>(status);
        PDH_FMT_COUNTERVALUE value{};
        const auto result = PdhGetFormattedCounterValue(counter_, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
                                                        nullptr, &value);
        if (result != ERROR_SUCCESS || !validCounter(value.CStatus) ||
            !std::isfinite(value.doubleValue) || value.doubleValue < 0) {
            fail(result == ERROR_SUCCESS ? static_cast<PDH_STATUS>(PDH_INVALID_DATA) : result);
            return missing<double>(status_);
        }
        return available(std::min(value.doubleValue, 100.0));
    }

    template<class T> CounterArray<T> array()
    {
        CounterArray<T> result;
        result.status = collect();
        if (result.status != SampleStatus::Ok)
            return result;
        constexpr DWORD format = (std::is_same_v<T, double> ? PDH_FMT_DOUBLE : PDH_FMT_LARGE) |
                                  PDH_FMT_NOCAP100 | PDH_FMT_NOSCALE;
        DWORD bytes = 0, count = 0;
        auto status = PdhGetFormattedCounterArrayW(counter_, format, &bytes, &count, nullptr);
        // Instance sets can change between sizing and reading. Bound both memory
        // and retries; an oversized or continuously changing set is unavailable.
        for (unsigned attempt = 0; attempt < 3 && static_cast<DWORD>(status) == PDH_MORE_DATA; ++attempt) {
            if (bytes == 0 || bytes > maxCounterBytes || count > maxCounterItems)
                break;
            buffer_.resize((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
            auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer_.data());
            status = PdhGetFormattedCounterArrayW(counter_, format, &bytes, &count, items);
            if (status != ERROR_SUCCESS)
                continue;
            if (count > maxCounterItems)
                break;
            result.items.reserve(count);
            for (DWORD i = 0; i < count; ++i) {
                if (!items[i].szName)
                    continue;
                NamedCounter<T> item;
                item.name = items[i].szName;
                const auto &value = items[i].FmtValue;
                if (validCounter(value.CStatus)) {
                    if constexpr (std::is_same_v<T, double>) {
                        if (std::isfinite(value.doubleValue) && value.doubleValue >= 0)
                            item.metric = available(value.doubleValue);
                    } else {
                        if (value.largeValue >= 0)
                            item.metric = available(static_cast<T>(value.largeValue));
                    }
                }
                result.items.push_back(std::move(item));
            }
            // No matching instance is not proof that the adapter is idle.
            result.status = result.items.empty() ? SampleStatus::Unavailable : SampleStatus::Ok;
            return result;
        }
        fail(status == ERROR_SUCCESS ? static_cast<PDH_STATUS>(PDH_INVALID_DATA) : status);
        result.status = status_;
        return result;
    }

private:
    SampleStatus collect()
    {
        if (!query_) {
            if (Clock::now() < retryAt_)
                return status_;
            auto status = PdhOpenQueryW(nullptr, 0, &query_);
            if (status == ERROR_SUCCESS)
                status = PdhAddEnglishCounterW(query_, path_, 0, &counter_);
            if (status != ERROR_SUCCESS) {
                fail(status);
                return status_;
            }
        }
        const auto status = PdhCollectQueryData(query_);
        if (status != ERROR_SUCCESS) {
            fail(status);
            return status_;
        }
        if (!collected_) {
            collected_ = true;
            if (rate_)
                return SampleStatus::WarmingUp;
        }
        status_ = SampleStatus::Ok;
        return status_;
    }

    void close()
    {
        if (query_)
            PdhCloseQuery(query_);
        query_ = nullptr;
        counter_ = nullptr;
        collected_ = false;
    }

    void fail(PDH_STATUS status)
    {
        close();
        status_ = counterFailure(status);
        retryAt_ = Clock::now() + retryInterval;
    }

    const wchar_t *path_;
    bool rate_;
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER counter_ = nullptr;
    bool collected_ = false;
    SampleStatus status_ = SampleStatus::Unavailable;
    Clock::time_point retryAt_{};
    std::vector<std::max_align_t> buffer_;
};

struct ReleaseCom {
    template<class T> void operator()(T *pointer) const { if (pointer) pointer->Release(); }
};

std::string utf8(const wchar_t *text)
{
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return "GPU";
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result.data(), size, nullptr, nullptr))
        return "GPU";
    result.pop_back();
    return result;
}

struct Adapter {
    std::uint64_t luid;
    std::string id;
    std::string name;
    std::uint64_t dedicatedBytes;
};

} // namespace

struct ResourceSampler::Impl {
    detail::SystemCpuDelta systemCpu;
    detail::ProcessCpuDelta processCpu;
    DWORD previousProcessorCount = 0;
    CounterQuery groupCpu{L"\\Processor Information(_Total)\\% Processor Time", true};
    CounterQuery gpuEngines{L"\\GPU Engine(*)\\Utilization Percentage", true};
    CounterQuery gpuMemory{L"\\GPU Adapter Memory(*)\\Dedicated Usage", false};
    std::unique_ptr<IDXGIFactory1, ReleaseCom> factory;
    std::vector<Adapter> adapters;
    SampleStatus adapterStatus = SampleStatus::Unavailable;
    Clock::time_point discoverAt{};

    void discoverAdapters()
    {
        const auto now = Clock::now();
        if (now < discoverAt && (!factory || factory->IsCurrent()))
            return;
        discoverAt = now + discoveryInterval;
        adapters.clear();
        adapterStatus = SampleStatus::Unavailable;
        IDXGIFactory1 *created = nullptr;
        const auto status = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&created));
        factory.reset(created);
        if (FAILED(status) || !factory)
            return;
        for (UINT index = 0; index <= maxAdapters; ++index) {
            IDXGIAdapter1 *rawAdapter = nullptr;
            const auto enumeration = factory->EnumAdapters1(index, &rawAdapter);
            std::unique_ptr<IDXGIAdapter1, ReleaseCom> adapter(rawAdapter);
            if (enumeration == DXGI_ERROR_NOT_FOUND) {
                adapterStatus = adapters.empty() ? SampleStatus::Unsupported : SampleStatus::Ok;
                return;
            }
            if (FAILED(enumeration) || !adapter || index == maxAdapters)
                break;
            DXGI_ADAPTER_DESC1 description{};
            if (FAILED(adapter->GetDesc1(&description)))
                break;
            if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;
            const std::uint64_t luid = (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(description.AdapterLuid.HighPart)) << 32) |
                description.AdapterLuid.LowPart;
            std::ostringstream id;
            id << std::hex << std::setfill('0') << std::setw(16) << luid;
            adapters.push_back({luid, id.str(), utf8(description.Description),
                                static_cast<std::uint64_t>(description.DedicatedVideoMemory)});
        }
        adapters.clear(); // Never present an incomplete enumeration as complete.
    }

    ResourceSnapshot sample()
    {
        ResourceSnapshot snapshot;
        const DWORD processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (processors != previousProcessorCount) {
            systemCpu.reset();
            previousProcessorCount = processors;
        }
        // GetSystemTimes only covers the caller's primary group on >64-CPU
        // machines. Use the all-group Processor Information total there.
        if (GetActiveProcessorGroupCount() > 1) {
            systemCpu.reset();
            snapshot.systemCpuPercent = groupCpu.scalar();
        } else {
            FILETIME idle{}, kernel{}, user{};
            snapshot.systemCpuPercent = GetSystemTimes(&idle, &kernel, &user) ?
                systemCpu.sample(detail::SystemCpuTimes{ticks(idle), ticks(kernel), ticks(user)}) :
                systemCpu.sample(std::nullopt);
        }

        FILETIME created{}, exited{}, kernel{}, user{};
        LARGE_INTEGER wall{}, frequency{};
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) &&
            QueryPerformanceCounter(&wall) && QueryPerformanceFrequency(&frequency) &&
            wall.QuadPart >= 0 && frequency.QuadPart > 0) {
            snapshot.processCpuPercent = processCpu.sample(detail::ProcessCpuTimes{
                ticks(kernel), ticks(user), static_cast<std::uint64_t>(wall.QuadPart),
                static_cast<std::uint64_t>(frequency.QuadPart), processors});
        } else {
            snapshot.processCpuPercent = processCpu.sample(std::nullopt);
        }

        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory)) {
            snapshot.systemMemoryUsedBytes = detail::physicalMemoryUsed(memory.ullTotalPhys, memory.ullAvailPhys);
            if (snapshot.systemMemoryUsedBytes.value)
                snapshot.systemMemoryTotalBytes = available<std::uint64_t>(memory.ullTotalPhys);
        }
        PROCESS_MEMORY_COUNTERS processMemory{};
        processMemory.cb = sizeof(processMemory);
        if (GetProcessMemoryInfo(GetCurrentProcess(), &processMemory, sizeof(processMemory)))
            snapshot.processWorkingSetBytes = available<std::uint64_t>(processMemory.WorkingSetSize);

        discoverAdapters();
        snapshot.gpuStatus = adapterStatus;
        if (adapterStatus != SampleStatus::Ok)
            return snapshot;

        const auto engineData = gpuEngines.array<double>();
        const auto memoryData = gpuMemory.array<std::uint64_t>();
        std::vector<detail::EngineCounter> engines;
        std::vector<detail::MemoryCounter> memories;
        bool unknownEngine = false, unknownMemory = false;
        for (const auto &counter : engineData.items) {
            const auto instance = detail::parseEngineInstance(counter.name);
            if (instance)
                engines.push_back({*instance, counter.metric});
            else
                unknownEngine = true;
        }
        for (const auto &counter : memoryData.items) {
            const auto instance = detail::parseMemoryInstance(counter.name);
            if (instance)
                memories.push_back({*instance, counter.metric});
            else
                unknownMemory = true;
        }
        for (const auto &adapter : adapters) {
            GpuSnapshot gpu;
            gpu.id = adapter.id;
            gpu.name = adapter.name;
            gpu.utilizationPercent = engineData.status == SampleStatus::Ok && !unknownEngine ?
                detail::busiestEngine(adapter.luid, engines) :
                missing<double>(unknownEngine ? SampleStatus::Unavailable : engineData.status);
            if (adapter.dedicatedBytes > 0) {
                gpu.dedicatedTotalBytes = available(adapter.dedicatedBytes);
                gpu.dedicatedUsedBytes = memoryData.status == SampleStatus::Ok && !unknownMemory ?
                    detail::dedicatedMemory(adapter.luid, memories) :
                    missing<std::uint64_t>(unknownMemory ? SampleStatus::Unavailable : memoryData.status);
            } else {
                gpu.dedicatedTotalBytes = missing<std::uint64_t>(SampleStatus::Unsupported);
                gpu.dedicatedUsedBytes = missing<std::uint64_t>(SampleStatus::Unsupported);
            }
            snapshot.gpus.push_back(std::move(gpu));
        }
        return snapshot;
    }
};

ResourceSampler::ResourceSampler() : impl_(std::make_unique<Impl>()) {}
ResourceSampler::~ResourceSampler() = default;
ResourceSnapshot ResourceSampler::sample() { return impl_->sample(); }
void ResourceSampler::reset() { impl_ = std::make_unique<Impl>(); }

} // namespace metrics

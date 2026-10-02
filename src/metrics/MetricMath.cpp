#include "MetricMath.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

namespace metrics::detail {

Metric<double> SystemCpuDelta::sample(std::optional<SystemCpuTimes> current)
{
    if (!current) {
        reset();
        return missing<double>();
    }
    const auto before = previous_;
    previous_ = current;
    if (!before || current->idle < before->idle || current->kernel < before->kernel ||
        current->user < before->user)
        return missing<double>(SampleStatus::WarmingUp);

    const auto idle = current->idle - before->idle;
    const auto kernel = current->kernel - before->kernel;
    const long double total = static_cast<long double>(kernel) +
                              static_cast<long double>(current->user - before->user);
    if (total == 0 || idle > kernel)
        return missing<double>();
    return available(static_cast<double>((total - idle) / total * 100.0L));
}

Metric<double> ProcessCpuDelta::sample(std::optional<ProcessCpuTimes> current)
{
    if (!current || current->logicalProcessors == 0 || current->wallFrequency == 0) {
        reset();
        return missing<double>();
    }
    const auto before = previous_;
    previous_ = current;
    if (!before || current->kernel100ns < before->kernel100ns ||
        current->user100ns < before->user100ns || current->wallTicks < before->wallTicks ||
        current->logicalProcessors != before->logicalProcessors ||
        current->wallFrequency != before->wallFrequency)
        return missing<double>(SampleStatus::WarmingUp);

    const auto elapsed = current->wallTicks - before->wallTicks;
    if (elapsed == 0) {
        previous_ = before; // Preserve CPU time until a measurable interval passes.
        return missing<double>();
    }
    const long double cpu = static_cast<long double>(current->kernel100ns - before->kernel100ns) +
                            static_cast<long double>(current->user100ns - before->user100ns);
    const long double capacity = static_cast<long double>(elapsed) /
                                 static_cast<long double>(current->wallFrequency) *
                                 10000000.0L * current->logicalProcessors;
    return available(static_cast<double>(std::clamp(cpu / capacity * 100.0L, 0.0L, 100.0L)));
}

Metric<std::uint64_t> physicalMemoryUsed(std::uint64_t total, std::uint64_t availableBytes)
{
    if (total == 0 || availableBytes > total)
        return missing<std::uint64_t>();
    return available(total - availableBytes);
}

namespace {

bool consume(std::wstring_view &text, std::wstring_view prefix)
{
    if (text.substr(0, prefix.size()) != prefix)
        return false;
    text.remove_prefix(prefix.size());
    return true;
}

bool number(std::wstring_view &text, std::uint32_t &out, unsigned base, std::size_t exactDigits = 0)
{
    std::uint64_t value = 0;
    std::size_t digits = 0;
    while (digits < text.size() && (!exactDigits || digits < exactDigits)) {
        const wchar_t c = text[digits];
        const unsigned digit = c >= L'0' && c <= L'9' ? static_cast<unsigned>(c - L'0') :
                               c >= L'a' && c <= L'f' ? static_cast<unsigned>(c - L'a') + 10 :
                               c >= L'A' && c <= L'F' ? static_cast<unsigned>(c - L'A') + 10 : base;
        if (digit >= base)
            break;
        value = value * base + digit;
        if (value > std::numeric_limits<std::uint32_t>::max())
            return false;
        ++digits;
    }
    if (digits == 0 || (exactDigits && digits != exactDigits))
        return false;
    text.remove_prefix(digits);
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool adapter(std::wstring_view &name, GpuInstance &instance)
{
    std::uint32_t high = 0, low = 0;
    if (!consume(name, L"luid_0x") || !number(name, high, 16, 8) ||
        !consume(name, L"_0x") || !number(name, low, 16, 8) ||
        !consume(name, L"_phys_") || !number(name, instance.physical, 10))
        return false;
    instance.luid = (static_cast<std::uint64_t>(high) << 32) | low;
    return true;
}

} // namespace

std::optional<GpuInstance> parseEngineInstance(std::wstring_view name)
{
    GpuInstance result{};
    if (!consume(name, L"pid_") || !number(name, result.process, 10) || !consume(name, L"_") ||
        !adapter(name, result) || !consume(name, L"_eng_") || !number(name, result.engine, 10) ||
        !consume(name, L"_engtype_") || name.empty())
        return std::nullopt;
    return result;
}

std::optional<GpuInstance> parseMemoryInstance(std::wstring_view name)
{
    GpuInstance result{};
    if (!adapter(name, result))
        return std::nullopt;
    if (!name.empty()) {
        std::uint32_t duplicate = 0;
        if (!consume(name, L"#") || !number(name, duplicate, 10) || !name.empty())
            return std::nullopt;
    }
    return result;
}

Metric<double> busiestEngine(std::uint64_t luid, const std::vector<EngineCounter> &counters)
{
    using ProcessEngine = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>;
    std::map<ProcessEngine, double> processes;
    for (const auto &counter : counters) {
        const auto &id = counter.instance;
        if (id.luid != luid)
            continue;
        if (counter.percent.status != SampleStatus::Ok || !counter.percent.value)
            return missing<double>(counter.percent.status == SampleStatus::WarmingUp ?
                                   SampleStatus::WarmingUp : SampleStatus::Unavailable);
        const double value = *counter.percent.value;
        if (!std::isfinite(value) || value < 0)
            return missing<double>();
        auto &process = processes[{id.physical, id.engine, id.process}];
        process = std::max(process, std::min(value, 100.0));
    }
    if (processes.empty())
        return missing<double>();
    std::map<std::pair<std::uint32_t, std::uint32_t>, double> engines;
    double busiest = 0;
    for (const auto &[key, percent] : processes) {
        auto &engine = engines[{std::get<0>(key), std::get<1>(key)}];
        engine = std::min(100.0, engine + percent);
        busiest = std::max(busiest, engine);
    }
    return available(busiest);
}

Metric<std::uint64_t> dedicatedMemory(std::uint64_t luid, const std::vector<MemoryCounter> &counters)
{
    std::map<std::uint32_t, std::uint64_t> physicalAdapters;
    for (const auto &counter : counters) {
        if (counter.instance.luid != luid)
            continue;
        if (counter.bytes.status != SampleStatus::Ok || !counter.bytes.value)
            return missing<std::uint64_t>();
        auto &bytes = physicalAdapters[counter.instance.physical];
        bytes = std::max(bytes, *counter.bytes.value); // Ignore duplicate aliases.
    }
    if (physicalAdapters.empty())
        return missing<std::uint64_t>();
    std::uint64_t total = 0;
    for (const auto &[physical, bytes] : physicalAdapters) {
        (void)physical;
        if (bytes > std::numeric_limits<std::uint64_t>::max() - total)
            return missing<std::uint64_t>();
        total += bytes;
    }
    return available(total);
}

} // namespace metrics::detail

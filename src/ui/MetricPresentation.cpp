#include "MetricPresentation.hpp"

#include <algorithm>
#include <cmath>

namespace monitor {
namespace {

constexpr const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};

struct ByteScale {
	double divisor = 1.0;
	int unit = 0;
};

ByteScale scaleFor(std::uint64_t bytes)
{
	ByteScale scale;
	while (static_cast<double>(bytes) / scale.divisor >= 1024.0 && scale.unit < 6) {
		scale.divisor *= 1024.0;
		++scale.unit;
	}
	return scale;
}

QString scaledNumber(std::uint64_t bytes, ByteScale scale, const QLocale &locale)
{
	return locale.toString(static_cast<double>(bytes) / scale.divisor, 'f', scale.unit == 0 ? 0 : 1);
}

} // namespace

QString byteText(std::uint64_t bytes, const QLocale &locale)
{
	const auto scale = scaleFor(bytes);
	return scaledNumber(bytes, scale, locale) + QStringLiteral(" ") + QString::fromLatin1(units[scale.unit]);
}

MetricDisplay displayPercent(const metrics::Metric<double> &metric, const QLocale &locale)
{
	if (metric.status != metrics::SampleStatus::Ok)
		return {{}, metric.status, std::nullopt};
	if (!metric.value || !std::isfinite(*metric.value) || *metric.value < 0.0 || *metric.value > 100.0)
		return {};

	return {locale.toString(*metric.value, 'f', 1) + locale.percent(), metrics::SampleStatus::Ok,
		static_cast<int>(std::lround(*metric.value * 10.0))};
}

MetricDisplay displayMemory(const metrics::Metric<std::uint64_t> &used,
			    const metrics::Metric<std::uint64_t> &total, bool showTotal, const QLocale &locale)
{
	if (used.status != metrics::SampleStatus::Ok)
		return {{}, used.status, std::nullopt};
	if (!used.value)
		return {};

	MetricDisplay display{byteText(*used.value, locale), metrics::SampleStatus::Ok, std::nullopt};
	if (total.status == metrics::SampleStatus::Ok && total.value && *total.value > 0) {
		const long double ratio = static_cast<long double>(*used.value) / *total.value;
		display.progress = static_cast<int>(std::lround(std::min(1.0L, ratio) * 1000.0L));
		if (showTotal) {
			const auto scale = scaleFor(std::max(*used.value, *total.value));
			display.text = scaledNumber(*used.value, scale, locale) + QStringLiteral(" / ") +
				       scaledNumber(*total.value, scale, locale) + QStringLiteral(" ") +
				       QString::fromLatin1(units[scale.unit]);
		}
	} else if (showTotal) {
		display.text += QStringLiteral(" / \u2014");
	}
	return display;
}

} // namespace monitor
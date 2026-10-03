#pragma once

#include "src/metrics/ResourceSampler.hpp"

#include <QLocale>
#include <QString>

#include <optional>

namespace monitor {

struct MetricDisplay {
	QString text;
	metrics::SampleStatus status = metrics::SampleStatus::Unavailable;
	// Tenths of a percent. An absent value must not be drawn as a zero bar.
	std::optional<int> progress;
};

QString byteText(std::uint64_t bytes, const QLocale &locale);
MetricDisplay displayPercent(const metrics::Metric<double> &metric, const QLocale &locale);
MetricDisplay displayMemory(const metrics::Metric<std::uint64_t> &used,
			    const metrics::Metric<std::uint64_t> &total, bool showTotal, const QLocale &locale);

} // namespace monitor
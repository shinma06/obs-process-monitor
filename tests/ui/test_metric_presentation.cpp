#include "src/ui/MetricPresentation.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

template<class T> metrics::Metric<T> ok(T value)
{
	return {value, metrics::SampleStatus::Ok};
}

void testStates()
{
	const auto locale = QLocale::c();
	auto zero = monitor::displayPercent(ok(0.0), locale);
	require(zero.text == QStringLiteral("0.0%") && zero.progress == 0, "a real zero is displayed");
	for (const auto state : {metrics::SampleStatus::WarmingUp, metrics::SampleStatus::Unavailable,
				 metrics::SampleStatus::Unsupported}) {
		const auto missing = monitor::displayPercent({42.0, state}, locale);
		require(missing.status == state && missing.text.isEmpty() && !missing.progress,
			"non-Ok state must discard even a supplied stale value");
	}
	for (const double bad : {-1.0, 100.1, std::numeric_limits<double>::infinity(),
				 std::numeric_limits<double>::quiet_NaN()}) {
		const auto invalid = monitor::displayPercent(ok(bad), locale);
		require(invalid.status == metrics::SampleStatus::Unavailable && !invalid.progress,
			"invalid rates must not become measured percentages");
	}
	const auto missing = monitor::displayPercent({std::nullopt, metrics::SampleStatus::Ok}, locale);
	require(missing.status == metrics::SampleStatus::Unavailable, "Ok without a value is invalid");
}

void testMemory()
{
	constexpr std::uint64_t GiB = 1024ULL * 1024 * 1024;
	const auto locale = QLocale::c();
	const auto memory = monitor::displayMemory(ok(GiB * 3 / 2), ok(GiB * 4), true, locale);
	require(memory.text == QStringLiteral("1.5 / 4.0 GiB") && memory.progress == 375,
		"binary units and total-memory denominator must be correct");
	const auto process = monitor::displayMemory(ok(GiB / 2), ok(GiB * 4), false, locale);
	require(process.text == QStringLiteral("512.0 MiB") && process.progress == 125,
		"working set has a byte value and system-memory denominator");
	const auto unknownTotal = monitor::displayMemory(ok(GiB), {}, true, locale);
	require(unknownTotal.status == metrics::SampleStatus::Ok && unknownTotal.text == QStringLiteral("1.0 GiB / \u2014") &&
			!unknownTotal.progress,
		"missing total preserves known usage and must not invent a bar");
	const auto zeroTotal = monitor::displayMemory(ok(GiB), ok(std::uint64_t(0)), true, locale);
	require(!zeroTotal.progress, "zero total must not divide by zero");
	const auto missingUsed = monitor::displayMemory({}, ok(GiB), true, locale);
	require(missingUsed.status == metrics::SampleStatus::Unavailable && missingUsed.text.isEmpty() &&
			!missingUsed.progress,
		"missing usage must not retain the previous value");
	const auto unsupported =
		monitor::displayMemory({GiB, metrics::SampleStatus::Unsupported}, ok(GiB), true, locale);
	require(unsupported.status == metrics::SampleStatus::Unsupported && !unsupported.progress,
		"unsupported usage must remain unsupported");
	const auto huge = monitor::displayMemory(ok(std::numeric_limits<std::uint64_t>::max()),
						ok(std::numeric_limits<std::uint64_t>::max()), true, locale);
	require(huge.progress == 1000 && huge.text.endsWith(QStringLiteral(" EiB")), "large counts do not overflow");
	const auto overCapacity = monitor::displayMemory(ok(GiB * 5), ok(GiB * 4), true, locale);
	require(overCapacity.text == QStringLiteral("5.0 / 4.0 GiB") && overCapacity.progress == 1000,
		"bar clamps while actual byte values remain visible");
	require(monitor::byteText(1024, locale) == QStringLiteral("1.0 KiB"), "KiB boundary uses binary units");
	require(monitor::byteText(0, locale) == QStringLiteral("0 B"), "zero bytes remain a measured zero");
}

void testLocale()
{
	const auto locale = QLocale(QLocale::German, QLocale::Germany);
	require(monitor::displayPercent(ok(12.5), locale).text == QStringLiteral("12,5%"),
		"numeric formatting follows locale");
	constexpr std::uint64_t GiB = 1024ULL * 1024 * 1024;
	require(monitor::displayMemory(ok(GiB * 3 / 2), ok(GiB * 4), true, locale).text ==
			QStringLiteral("1,5 / 4,0 GiB"),
		"memory formatting follows locale");
}

} // namespace

int main()
{
	try {
		testStates();
		testMemory();
		testLocale();
		std::cout << "Synthetic metric presentation cases passed.\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
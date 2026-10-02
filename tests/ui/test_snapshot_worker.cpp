#include "src/ui/SnapshotWorker.hpp"

#include <algorithm>
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;

namespace {

void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

template<class Predicate> bool eventually(Predicate predicate)
{
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline) {
		if (predicate())
			return true;
		std::this_thread::sleep_for(1ms);
	}
	return predicate();
}

metrics::ResourceSnapshot snapshot(double value)
{
	metrics::ResourceSnapshot result;
	result.systemCpuPercent = {value, metrics::SampleStatus::Ok};
	return result;
}

void testPauseResumeAndThread()
{
	std::mutex mutex;
	std::vector<bool> resets;
	std::thread::id sampleThread;
	monitor::SnapshotWorker worker([&](bool reset) {
		std::lock_guard lock(mutex);
		sampleThread = std::this_thread::get_id();
		resets.push_back(reset);
		return snapshot(12.5);
	}, 10ms);
	require(!worker.latest().snapshot, "worker starts paused without invented data");
	worker.setActive(true);
	require(eventually([&] {
		std::lock_guard lock(mutex);
		return resets.size() >= 2;
	}), "worker must take samples");
	worker.setActive(false);
	require(!worker.latest().snapshot, "pausing clears the published sample");
	{
		std::lock_guard lock(mutex);
		require(sampleThread != std::this_thread::get_id(), "sampling must be off the owner thread");
		require(resets.front() && !resets[1], "only the first active sample resets its baseline");
	}
	std::size_t before;
	{
		std::lock_guard lock(mutex);
		before = resets.size();
	}
	worker.setActive(true);
	require(eventually([&] {
		std::lock_guard lock(mutex);
		return resets.size() > before && std::any_of(resets.begin() + before, resets.end(), [](bool reset) { return reset; });
	}), "resuming resets the rate baseline");
	worker.stop();
	worker.stop();
	worker.setActive(true);
	std::size_t stoppedCount;
	{
		std::lock_guard lock(mutex);
		stoppedCount = resets.size();
	}
	std::this_thread::sleep_for(30ms);
	{
		std::lock_guard lock(mutex);
		require(resets.size() == stoppedCount, "stopped worker must never restart");
	}
}

void testInFlightGenerationAndStop()
{
	std::promise<void> entered;
	std::promise<void> enteredSecond;
	std::promise<void> release;
	std::promise<void> releaseSecond;
	auto releaseFuture = release.get_future().share();
	auto releaseSecondFuture = releaseSecond.get_future().share();
	std::atomic<int> calls{0};
	monitor::SnapshotWorker worker([&](bool) {
		const int call = ++calls;
		if (call == 1) {
			entered.set_value();
			releaseFuture.wait();
		}
		if (call == 2) {
			enteredSecond.set_value();
			releaseSecondFuture.wait();
		}
		return snapshot(call == 1 ? 11.0 : 22.0);
	}, 10ms);
	worker.setActive(true);
	const bool started = entered.get_future().wait_for(2s) == std::future_status::ready;
	if (!started) {
		release.set_value();
		require(false, "first sample must start");
	}
	worker.setActive(false);
	worker.setActive(true);
	release.set_value();
	const bool secondStarted = enteredSecond.get_future().wait_for(2s) == std::future_status::ready;
	const bool oldDiscarded = !worker.latest().snapshot;
	releaseSecond.set_value();
	require(secondStarted && oldDiscarded, "old generation must not publish while the resumed sample is in flight");
	require(eventually([&] {
		const auto update = worker.latest();
		return update.snapshot && update.snapshot->systemCpuPercent.value == 22.0;
	}), "sample from the old visibility generation must be discarded");
	worker.stop();

	std::promise<void> enteredAgain;
	std::promise<void> releaseAgain;
	auto releaseAgainFuture = releaseAgain.get_future().share();
	monitor::SnapshotWorker joining([&](bool) {
		enteredAgain.set_value();
		releaseAgainFuture.wait();
		return snapshot(0.0);
	}, 1s);
	joining.setActive(true);
	const bool joiningStarted = enteredAgain.get_future().wait_for(2s) == std::future_status::ready;
	if (!joiningStarted) {
		releaseAgain.set_value();
		require(false, "joining sample must start");
	}
	auto stopped = std::async(std::launch::async, [&] { joining.stop(); });
	const bool waited = stopped.wait_for(30ms) == std::future_status::timeout;
	releaseAgain.set_value();
	const bool completed = stopped.wait_for(2s) == std::future_status::ready;
	require(waited && completed, "stop must join an in-flight sample without detaching");
	stopped.get();
}

void testFailureClearsSample()
{
	std::atomic<int> calls{0};
	std::atomic<bool> recoveredWithReset{false};
	monitor::SnapshotWorker worker([&](bool reset) {
		const int call = ++calls;
		if (call == 1)
			return snapshot(50.0);
		if (call == 2)
			throw std::runtime_error("synthetic acquisition failure");
		if (call == 3)
			recoveredWithReset = reset;
		return snapshot(25.0);
	}, 50ms);
	worker.setActive(true);
	require(eventually([&] {
		const auto latest = worker.latest();
		return latest.snapshot && latest.snapshot->systemCpuPercent.value == 50.0;
	}), "initial fixture must publish");
	require(eventually([&] {
		const auto latest = worker.latest();
		return latest.snapshot && latest.snapshot->systemCpuPercent.status == metrics::SampleStatus::Unavailable &&
		       !latest.snapshot->systemCpuPercent.value;
	}), "an acquisition exception must publish unavailable, not stale data");
	require(eventually([&] {
		const auto latest = worker.latest();
		return latest.snapshot && latest.snapshot->systemCpuPercent.value == 25.0;
	}), "worker must recover from an acquisition failure");
	require(recoveredWithReset, "a failed sample resets before recovery");
	worker.stop();
}

} // namespace

int main()
{
	try {
		testPauseResumeAndThread();
		testInFlightGenerationAndStop();
		testFailureClearsSample();
		std::cout << "Synthetic worker pause, recovery and joined-shutdown cases passed.\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
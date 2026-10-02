#include "SnapshotWorker.hpp"

#include <algorithm>
#include <utility>

namespace monitor {

SnapshotWorker::SnapshotWorker(SampleFunction sample, std::chrono::milliseconds interval)
	: thread_([this, sample = std::move(sample), interval]() mutable {
		  run(std::move(sample), std::max(interval, std::chrono::milliseconds(1)));
	  })
{
}

SnapshotWorker::~SnapshotWorker()
{
	stop();
}

void SnapshotWorker::setActive(bool active)
{
	{
		std::lock_guard lock(mutex_);
		if (stopping_ || active_ == active)
			return;
		active_ = active;
		++generation_;
		latest_.snapshot.reset();
		++latest_.revision;
	}
	wake_.notify_one();
}

SnapshotWorker::Update SnapshotWorker::latest() const
{
	std::lock_guard lock(mutex_);
	return latest_;
}

void SnapshotWorker::stop()
{
	{
		std::lock_guard lock(mutex_);
		stopping_ = true;
		active_ = false;
	}
	wake_.notify_one();
	if (thread_.joinable())
		thread_.join();
}

void SnapshotWorker::run(SampleFunction sample, std::chrono::milliseconds interval)
{
	std::unique_lock lock(mutex_);
	std::uint64_t sampledGeneration = 0;
	while (!stopping_) {
		wake_.wait(lock, [this] { return stopping_ || active_; });
		if (stopping_)
			break;
		const auto generation = generation_;
		const bool reset = sampledGeneration != generation;
		sampledGeneration = generation;
		lock.unlock();

		metrics::ResourceSnapshot snapshot;
		try {
			snapshot = sample(reset);
		} catch (...) {
			// An unavailable snapshot clears previously displayed values. Retry
			// after the normal interval with a fresh rate baseline.
			sampledGeneration = 0;
		}
		const auto sampledAt = Clock::now();
		std::shared_ptr<const metrics::ResourceSnapshot> shared;
		try {
			shared = std::make_shared<const metrics::ResourceSnapshot>(std::move(snapshot));
		} catch (...) {
			// Allocation failure must not terminate the host. The GUI's stale
			// timeout removes the last sample while this worker stops.
			return;
		}

		lock.lock();
		if (!stopping_ && active_ && generation_ == generation) {
			latest_.snapshot = std::move(shared);
			latest_.sampledAt = sampledAt;
			++latest_.revision;
		}
		wake_.wait_for(lock, interval, [this, generation] {
			return stopping_ || !active_ || generation_ != generation;
		});
	}
}

} // namespace monitor
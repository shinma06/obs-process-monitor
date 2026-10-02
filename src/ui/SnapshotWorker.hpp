#pragma once

#include "src/metrics/ResourceSampler.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace monitor {

// Owns no Qt objects and never posts callbacks to the dock. The GUI only copies
// the latest immutable snapshot, so delayed GUI events cannot queue samples.
class SnapshotWorker {
public:
	using SampleFunction = std::function<metrics::ResourceSnapshot(bool resetBaseline)>;
	using Clock = std::chrono::steady_clock;
	struct Update {
		std::shared_ptr<const metrics::ResourceSnapshot> snapshot;
		Clock::time_point sampledAt{};
		std::uint64_t revision = 0;
	};

	explicit SnapshotWorker(SampleFunction sample,
				std::chrono::milliseconds interval = std::chrono::seconds(1));
	~SnapshotWorker();
	SnapshotWorker(const SnapshotWorker &) = delete;
	SnapshotWorker &operator=(const SnapshotWorker &) = delete;

	void setActive(bool active);
	Update latest() const;
	// Called by the GUI owner before module unload. Joins an in-flight OS sample;
	// never detaches a thread that could continue executing unloaded plugin code.
	void stop();

private:
	void run(SampleFunction sample, std::chrono::milliseconds interval);
	mutable std::mutex mutex_;
	std::condition_variable wake_;
	bool active_ = false;
	bool stopping_ = false;
	std::uint64_t generation_ = 0;
	Update latest_;
	std::thread thread_;
};

} // namespace monitor
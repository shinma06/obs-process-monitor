#include "ProcessMonitorWidget.hpp"

#include "src/metrics/ResourceSampler.hpp"
#include "src/ui/MetricPresentation.hpp"
#include "src/ui/SnapshotWorker.hpp"

#include <obs-module.h>

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHideEvent>
#include <QLabel>
#include <QProgressBar>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <array>
#include <chrono>
#include <system_error>

namespace {

QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QString stateText(metrics::SampleStatus status)
{
	switch (status) {
	case metrics::SampleStatus::WarmingUp:
		return text("ProcessMonitor.Collecting");
	case metrics::SampleStatus::Unsupported:
		return text("ProcessMonitor.Unsupported");
	case metrics::SampleStatus::Unavailable:
		return text("ProcessMonitor.Unavailable");
	case metrics::SampleStatus::Ok:
		break;
	}
	return text("ProcessMonitor.Unavailable");
}

QLabel *plainLabel(QWidget *parent)
{
	auto *label = new QLabel(parent);
	// Adapter names are driver data and must never be interpreted as rich text.
	label->setTextFormat(Qt::PlainText);
	label->setWordWrap(true);
	return label;
}

struct MetricRow {
	QLabel *value = nullptr;
	QProgressBar *bar = nullptr;
	QString description;

	void display(const monitor::MetricDisplay &metric)
	{
		const QString valueText = metric.status == metrics::SampleStatus::Ok ? metric.text : stateText(metric.status);
		if (value->text() != valueText)
			value->setText(valueText);
		value->setAccessibleDescription(description + QStringLiteral(" ") + valueText);
		bar->setVisible(metric.progress.has_value());
		if (metric.progress) {
			bar->setValue(*metric.progress);
			bar->setAccessibleDescription(description + QStringLiteral(" ") + valueText);
		} else {
			// Hidden bars retain their layout space, but never retain a stale fill.
			bar->reset();
		}
	}
};

MetricRow addMetric(QVBoxLayout *layout, const QString &scope, const char *nameKey, const char *descriptionKey,
		    const char *objectName)
{
	QWidget *parent = layout->parentWidget();
	auto *form = new QFormLayout();
	form->setContentsMargins(0, 0, 0, 0);
	form->setRowWrapPolicy(QFormLayout::WrapLongRows);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

	auto *name = plainLabel(parent);
	name->setText(text(nameKey));
	auto *value = plainLabel(parent);
	value->setObjectName(QString::fromLatin1(objectName) + QStringLiteral("Value"));
	value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	const QString accessibleName = scope + QStringLiteral(" ") + name->text();
	const QString description = text(descriptionKey);
	value->setAccessibleName(accessibleName);
	name->setToolTip(description);
	value->setToolTip(description);

	auto *bar = new QProgressBar(parent);
	bar->setObjectName(QString::fromLatin1(objectName) + QStringLiteral("Bar"));
	bar->setRange(0, 1000);
	bar->setTextVisible(false);
	bar->setAccessibleName(accessibleName);
	bar->setToolTip(description);
	auto policy = bar->sizePolicy();
	policy.setRetainSizeWhenHidden(true);
	bar->setSizePolicy(policy);
	form->addRow(name, value);
	form->addRow(bar);
	layout->addLayout(form);
	return {value, bar, description};
}

} // namespace

struct ProcessMonitorWidget::Impl {
	enum Row { SystemCpu, SystemMemory, ProcessCpu, ProcessMemory, GpuUsage, GpuMemory, RowCount };

	explicit Impl(ProcessMonitorWidget *owner) : owner(owner)
	{
		owner->setObjectName(QStringLiteral("processMonitor"));
		owner->setAccessibleName(text("ProcessMonitor.DockTitle"));

		auto *outer = new QVBoxLayout(owner);
		outer->setContentsMargins(0, 0, 0, 0);
		auto *scroll = new QScrollArea(owner);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setWidgetResizable(true);
		auto *contents = new QWidget(scroll);
		auto *layout = new QVBoxLayout(contents);
		// Qt style supplies all inner spacing and margins, as in the OBS Stats dock.
		auto addGroup = [&](const char *key, const char *objectName) {
			auto *group = new QGroupBox(text(key), contents);
			group->setObjectName(QString::fromLatin1(objectName));
			auto *groupLayout = new QVBoxLayout(group);
			layout->addWidget(group);
			return groupLayout;
		};

		auto *system = addGroup("ProcessMonitor.System", "systemGroup");
		rows[SystemCpu] = addMetric(system, text("ProcessMonitor.System"), "ProcessMonitor.CPU",
					   "ProcessMonitor.SystemCpuHelp", "systemCpu");
		rows[SystemMemory] = addMetric(system, text("ProcessMonitor.System"), "ProcessMonitor.RAM",
					      "ProcessMonitor.SystemMemoryHelp", "systemMemory");

		auto *process = addGroup("ProcessMonitor.Process", "processGroup");
		rows[ProcessCpu] = addMetric(process, text("ProcessMonitor.Process"), "ProcessMonitor.CPU",
					    "ProcessMonitor.ProcessCpuHelp", "processCpu");
		rows[ProcessMemory] = addMetric(process, text("ProcessMonitor.Process"), "ProcessMonitor.RAM",
					       "ProcessMonitor.ProcessMemoryHelp", "processMemory");

		auto *gpu = addGroup("ProcessMonitor.GPU", "gpuGroup");
		adapterName = plainLabel(gpu->parentWidget());
		adapterName->setObjectName(QStringLiteral("adapterName"));
		adapterName->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		gpu->addWidget(adapterName);
		adapter = new QComboBox(gpu->parentWidget());
		adapter->setObjectName(QStringLiteral("gpuAdapter"));
		adapter->setAccessibleName(text("ProcessMonitor.Adapter"));
		adapter->setToolTip(text("ProcessMonitor.AdapterHelp"));
		adapter->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		adapter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		gpu->addWidget(adapter);
		rows[GpuUsage] = addMetric(gpu, text("ProcessMonitor.GPU"), "ProcessMonitor.Usage",
					  "ProcessMonitor.GpuHelp", "gpuUsage");
		rows[GpuMemory] = addMetric(gpu, text("ProcessMonitor.GPU"), "ProcessMonitor.VRAM",
					   "ProcessMonitor.GpuMemoryHelp", "gpuMemory");

		status = plainLabel(contents);
		status->setObjectName(QStringLiteral("monitorStatus"));
		status->setToolTip(text("ProcessMonitor.UpdateHelp"));
		status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->addWidget(status);
		layout->addStretch();
		scroll->setWidget(contents);
		outer->addWidget(scroll);

		QObject::connect(adapter, &QComboBox::currentIndexChanged, owner, [this] { renderGpu(); });
		timer = new QTimer(owner);
		timer->setInterval(1000);
		QObject::connect(timer, &QTimer::timeout, owner, [this] { refresh(); });
		clear(metrics::SampleStatus::WarmingUp);
		adapter->hide();
		adapterName->setText(text("ProcessMonitor.DetectingAdapters"));
		status->setText(text("ProcessMonitor.Collecting"));

		try {
			worker = std::make_unique<monitor::SnapshotWorker>(
				[sampler = std::shared_ptr<metrics::ResourceSampler>{}](bool reset) mutable {
					// Construction, reset, all OS sampling and destruction happen on
					// the worker thread, including adapter discovery.
					if (!sampler)
						sampler = std::make_shared<metrics::ResourceSampler>();
					else if (reset)
						sampler->reset();
					return sampler->sample();
				});
		} catch (const std::system_error &) {
			clear(metrics::SampleStatus::Unavailable);
			adapterName->setText(text("ProcessMonitor.Unavailable"));
			status->setText(text("ProcessMonitor.StartFailed"));
		}
	}

	void clear(metrics::SampleStatus sampleStatus)
	{
		for (auto &row : rows)
			row.display({{}, sampleStatus, std::nullopt});
	}

	void resume()
	{
		if (stopped || !worker)
			return;
		current.reset();
		stale = false;
		shownAt = monitor::SnapshotWorker::Clock::now();
		clear(metrics::SampleStatus::WarmingUp);
		adapter->hide();
		adapterName->show();
		adapterName->setText(text("ProcessMonitor.DetectingAdapters"));
		status->setText(text("ProcessMonitor.Collecting"));
		worker->setActive(true);
		timer->start();
	}

	void pause()
	{
		timer->stop();
		if (worker)
			worker->setActive(false);
	}

	void stop()
	{
		if (stopped)
			return;
		stopped = true;
		timer->stop();
		if (worker)
			worker->stop();
	}

	void refresh()
	{
		if (!worker || stopped)
			return;
		const auto update = worker->latest();
		const auto now = monitor::SnapshotWorker::Clock::now();
		const auto last = update.snapshot ? update.sampledAt : shownAt;
		if (now - last > std::chrono::seconds(3)) {
			if (!stale) {
				stale = true;
				clear(metrics::SampleStatus::Unavailable);
				status->setText(text("ProcessMonitor.WaitingForUpdate"));
			}
			return;
		}
		if (!update.snapshot || (revision == update.revision && !stale))
			return;
		revision = update.revision;
		stale = false;
		current = update.snapshot;
		const auto locale = owner->locale();
		rows[SystemCpu].display(monitor::displayPercent(current->systemCpuPercent, locale));
		rows[ProcessCpu].display(monitor::displayPercent(current->processCpuPercent, locale));
		rows[SystemMemory].display(monitor::displayMemory(current->systemMemoryUsedBytes,
							       current->systemMemoryTotalBytes, true, locale));
		rows[ProcessMemory].display(monitor::displayMemory(current->processWorkingSetBytes,
								current->systemMemoryTotalBytes, false, locale));

		const QString selected = adapter->currentData().toString();
		bool adaptersChanged = adapter->count() != static_cast<int>(current->gpus.size());
		for (int i = 0; !adaptersChanged && i < adapter->count(); ++i) {
			const auto &gpu = current->gpus[static_cast<std::size_t>(i)];
			adaptersChanged = adapter->itemData(i).toString() != QString::fromStdString(gpu.id) ||
					  adapter->itemText(i) != gpuName(gpu, i);
		}
		if (adaptersChanged) {
			const QSignalBlocker blocker(adapter);
			adapter->clear();
			for (std::size_t i = 0; i < current->gpus.size(); ++i) {
				const auto &gpu = current->gpus[i];
				adapter->addItem(gpuName(gpu, static_cast<int>(i)), QString::fromStdString(gpu.id));
			}
			const int index = adapter->findData(selected);
			adapter->setCurrentIndex(index >= 0 ? index : 0);
		}
		renderGpu();
		const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - update.sampledAt).count();
		const QTime sampledTime = QTime::currentTime().addMSecs(-static_cast<int>(age));
		status->setText(text("ProcessMonitor.Updated").arg(locale.toString(sampledTime, QStringLiteral("HH:mm:ss"))));
	}

	QString gpuName(const metrics::GpuSnapshot &gpu, int index) const
	{
		return gpu.name.empty() ? text("ProcessMonitor.AdapterNumber").arg(index + 1) :
					  QString::fromUtf8(gpu.name.data(), static_cast<qsizetype>(gpu.name.size()));
	}

	void renderGpu()
	{
		if (!current)
			return;
		if (stale) {
			rows[GpuUsage].display({});
			rows[GpuMemory].display({});
			return;
		}
		if (current->gpus.empty()) {
			adapter->hide();
			adapterName->show();
			adapterName->setText(stateText(current->gpuStatus));
			rows[GpuUsage].display({{}, current->gpuStatus, std::nullopt});
			rows[GpuMemory].display({{}, current->gpuStatus, std::nullopt});
			return;
		}
		const int index = adapter->currentIndex();
		if (index < 0 || static_cast<std::size_t>(index) >= current->gpus.size())
			return;
		const auto &gpu = current->gpus[static_cast<std::size_t>(index)];
		adapter->setVisible(current->gpus.size() > 1);
		adapterName->setVisible(current->gpus.size() == 1);
		adapterName->setText(gpuName(gpu, index));
		adapterName->setToolTip(text("ProcessMonitor.AdapterHelp"));
		adapter->setToolTip(gpuName(gpu, index) + QStringLiteral("\n") + text("ProcessMonitor.AdapterHelp"));
		const auto locale = owner->locale();
		rows[GpuUsage].display(monitor::displayPercent(gpu.utilizationPercent, locale));
		rows[GpuMemory].display(monitor::displayMemory(gpu.dedicatedUsedBytes, gpu.dedicatedTotalBytes, true, locale));
	}

	ProcessMonitorWidget *owner;
	std::array<MetricRow, RowCount> rows;
	QLabel *adapterName = nullptr;
	QComboBox *adapter = nullptr;
	QLabel *status = nullptr;
	QTimer *timer = nullptr;
	std::unique_ptr<monitor::SnapshotWorker> worker;
	std::shared_ptr<const metrics::ResourceSnapshot> current;
	monitor::SnapshotWorker::Clock::time_point shownAt{};
	std::uint64_t revision = 0;
	bool stopped = false;
	bool stale = false;
};

ProcessMonitorWidget::ProcessMonitorWidget(QWidget *parent) : QWidget(parent), impl_(std::make_unique<Impl>(this)) {}

ProcessMonitorWidget::~ProcessMonitorWidget()
{
	stopMonitoring();
}

void ProcessMonitorWidget::stopMonitoring()
{
	impl_->stop();
}

void ProcessMonitorWidget::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	impl_->resume();
}

void ProcessMonitorWidget::hideEvent(QHideEvent *event)
{
	impl_->pause();
	QWidget::hideEvent(event);
}
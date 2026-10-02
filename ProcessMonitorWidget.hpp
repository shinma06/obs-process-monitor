#pragma once

#include <QWidget>

#include <memory>

class ProcessMonitorWidget : public QWidget {
	Q_OBJECT

public:
	explicit ProcessMonitorWidget(QWidget *parent = nullptr);
	~ProcessMonitorWidget() override;

	// Must run on the GUI thread before removing the dock or unloading its module.
	// Idempotent; returns only after the sampler has stopped.
	void stopMonitoring();

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
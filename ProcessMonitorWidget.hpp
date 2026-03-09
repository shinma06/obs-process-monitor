#pragma once

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>

// Windows process metrics via Win32 API
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

class ProcessMonitorWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProcessMonitorWidget(QWidget *parent = nullptr);
    ~ProcessMonitorWidget() override;

private slots:
    void refresh();

private:
    // Returns CPU usage [0.0, 100.0] for the current process, or -1.0 on error.
    double calcCpuUsage();

    void styleBar(QProgressBar *bar, double pct);

    // UI elements
    QProgressBar *cpuBar;
    QLabel       *cpuValue;
    QProgressBar *ramBar;
    QLabel       *ramValue;
    QLabel       *statusLabel;
    QTimer       *timer;

    // CPU measurement state (Win32)
    ULARGE_INTEGER prevWall;   // system-wide wall time
    ULARGE_INTEGER prevKernel; // process kernel time
    ULARGE_INTEGER prevUser;   // process user time
    int numCores;
    HANDLE hProcess;
};

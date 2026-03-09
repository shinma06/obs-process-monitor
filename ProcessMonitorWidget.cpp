#include "ProcessMonitorWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QString>
#include <QDateTime>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
ProcessMonitorWidget::ProcessMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    // --- Win32 init ---
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    numCores  = static_cast<int>(si.dwNumberOfProcessors);
    hProcess  = GetCurrentProcess(); // OBS process itself

    // Seed the CPU timing baseline
    FILETIME ftWall, ftCreate, ftExit, ftKernel, ftUser;
    GetSystemTimeAsFileTime(&ftWall);
    memcpy(&prevWall, &ftWall, sizeof(FILETIME));

    if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        memcpy(&prevKernel, &ftKernel, sizeof(FILETIME));
        memcpy(&prevUser,   &ftUser,   sizeof(FILETIME));
    } else {
        prevKernel.QuadPart = 0;
        prevUser.QuadPart   = 0;
    }

    // --- UI ---
    setMinimumWidth(180);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // Title
    auto *title = new QLabel("OBS Process Monitor", this);
    title->setStyleSheet(
        "font-size: 11px; font-weight: bold;"
        "color: #7aa2f7; padding-bottom: 2px;");
    root->addWidget(title);

    // Divider
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #292e42;");
    root->addWidget(line);

    // Helper lambda: build one metric row (label | bar | value)
    auto makeRow = [&](const QString &name,
                       QProgressBar *&bar,
                       QLabel       *&valueLabel) {
        auto *row    = new QHBoxLayout();
        auto *lbl    = new QLabel(name, this);
        lbl->setFixedWidth(34);
        lbl->setStyleSheet("color: #a9b1d6; font-size: 11px;");

        bar = new QProgressBar(this);
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setTextVisible(false);
        bar->setFixedHeight(10);
        bar->setStyleSheet(
            "QProgressBar {"
            "  background: #292e42;"
            "  border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "  background: #7aa2f7;"
            "  border-radius: 4px;"
            "}");

        valueLabel = new QLabel("--", this);
        valueLabel->setFixedWidth(58);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setStyleSheet("color: #c0caf5; font-size: 11px;");

        row->addWidget(lbl);
        row->addWidget(bar);
        row->addWidget(valueLabel);
        root->addLayout(row);
    };

    makeRow("CPU", cpuBar, cpuValue);
    makeRow("RAM", ramBar, ramValue);

    root->addStretch();

    // Status line
    statusLabel = new QLabel("--", this);
    statusLabel->setStyleSheet("color: #565f89; font-size: 9px;");
    root->addWidget(statusLabel);

    // --- Timer ---
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ProcessMonitorWidget::refresh);
    timer->start(1000); // 1 Hz
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
ProcessMonitorWidget::~ProcessMonitorWidget()
{
    timer->stop();
}

// ---------------------------------------------------------------------------
// CPU usage for current process (Win32 GetProcessTimes diff)
//
// Formula:
//   cpu% = (kernelDiff + userDiff) / (wallDiff * numCores) * 100
//
// All times are in 100-nanosecond intervals (FILETIME units).
// ---------------------------------------------------------------------------
double ProcessMonitorWidget::calcCpuUsage()
{
    FILETIME ftWall, ftCreate, ftExit, ftKernel, ftUser;
    ULARGE_INTEGER nowWall, nowKernel, nowUser;

    // Current wall time
    GetSystemTimeAsFileTime(&ftWall);
    memcpy(&nowWall, &ftWall, sizeof(FILETIME));

    // Current process CPU time
    if (!GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser))
        return -1.0;

    memcpy(&nowKernel, &ftKernel, sizeof(FILETIME));
    memcpy(&nowUser,   &ftUser,   sizeof(FILETIME));

    // Deltas (100-ns units)
    ULONGLONG wallDiff   = nowWall.QuadPart   - prevWall.QuadPart;
    ULONGLONG cpuDiff    = (nowKernel.QuadPart - prevKernel.QuadPart)
                         + (nowUser.QuadPart   - prevUser.QuadPart);

    // Update state for next call
    prevWall   = nowWall;
    prevKernel = nowKernel;
    prevUser   = nowUser;

    if (wallDiff == 0 || numCores == 0)
        return 0.0;

    double pct = static_cast<double>(cpuDiff)
               / static_cast<double>(wallDiff * numCores)
               * 100.0;

    // Clamp [0, 100]
    if (pct < 0.0)  pct = 0.0;
    if (pct > 100.0) pct = 100.0;

    return pct;
}

// ---------------------------------------------------------------------------
// Apply color coding based on usage percentage
// ---------------------------------------------------------------------------
void ProcessMonitorWidget::styleBar(QProgressBar *bar, double pct)
{
    QString color;
    if (pct >= 90.0)
        color = "#f7768e"; // danger: red
    else if (pct >= 70.0)
        color = "#e0af68"; // warn: orange
    else
        color = "#7aa2f7"; // normal: blue

    bar->setStyleSheet(QString(
        "QProgressBar {"
        "  background: #292e42;"
        "  border-radius: 4px;"
        "}"
        "QProgressBar::chunk {"
        "  background: %1;"
        "  border-radius: 4px;"
        "}").arg(color));
}

// ---------------------------------------------------------------------------
// Periodic refresh (called by QTimer every 1 s)
// ---------------------------------------------------------------------------
void ProcessMonitorWidget::refresh()
{
    // --- CPU ---
    double cpu = calcCpuUsage();
    if (cpu >= 0.0) {
        int cpuInt = static_cast<int>(cpu + 0.5);
        cpuBar->setValue(cpuInt);
        cpuValue->setText(QString("%1%").arg(cpu, 0, 'f', 1));
        styleBar(cpuBar, cpu);
    } else {
        cpuValue->setText("err");
    }

    // --- RAM (Working Set = physical pages mapped to this process) ---
    PROCESS_MEMORY_COUNTERS_EX pmc;
    ZeroMemory(&pmc, sizeof(pmc));
    pmc.cb = sizeof(pmc);

    if (GetProcessMemoryInfo(
            hProcess,
            reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc),
            sizeof(pmc)))
    {
        // WorkingSetSize: physical RAM pages currently resident
        double ramMB = static_cast<double>(pmc.WorkingSetSize)
                     / (1024.0 * 1024.0);

        // Calculate % against total physical memory (for bar scale)
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        double totalMB = static_cast<double>(ms.ullTotalPhys)
                       / (1024.0 * 1024.0);
        double ramPct  = (totalMB > 0.0)
                       ? (ramMB / totalMB * 100.0)
                       : 0.0;
        if (ramPct > 100.0) ramPct = 100.0;

        ramBar->setValue(static_cast<int>(ramPct + 0.5));

        // Display in MB or GB depending on size
        if (ramMB >= 1024.0)
            ramValue->setText(QString("%1GB")
                .arg(ramMB / 1024.0, 0, 'f', 2));
        else
            ramValue->setText(QString("%1MB")
                .arg(ramMB, 0, 'f', 0));

        styleBar(ramBar, ramPct);
    } else {
        ramValue->setText("err");
    }

    // Status timestamp
    statusLabel->setText(
        QDateTime::currentDateTime().toString("hh:mm:ss"));
}

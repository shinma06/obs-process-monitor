# obs-process-monitor

An OBS Studio plugin that displays the **OBS process's own** CPU and RAM usage
as a dockable panel inside the OBS UI.

No Node.js, no WSL, no external tools required at runtime — just a single `.dll`.

---

## Features

- CPU usage % for the OBS process (via `GetProcessTimes`)
- RAM working set in MB/GB (via `GetProcessMemoryInfo`)
- Color-coded bars (blue → orange → red at 70% / 90%)
- Fully dockable — drag it anywhere in the OBS UI or leave it floating
- Persists dock position across OBS restarts
- 1-second refresh rate, minimal overhead

---

## Requirements (build only — not runtime)

| Tool | Version |
|------|---------|
| Git | any |
| CMake | 3.16+ |
| Visual Studio | 2022 (with "Desktop development with C++") |

All OBS headers, Qt, and other dependencies are **downloaded automatically** by CMake.

---

## Build Steps

### 1. Clone the official OBS plugin template

```cmd
git clone https://github.com/obsproject/obs-plugintemplate.git obs-process-monitor
cd obs-process-monitor
```

### 2. Replace source files

Copy the files from this repository **over** the cloned template:

```
CMakeLists.txt          -> obs-process-monitor/CMakeLists.txt      (replace)
buildspec.json          -> obs-process-monitor/buildspec.json       (replace)
src/plugin-main.cpp     -> obs-process-monitor/src/plugin-main.cpp  (replace)
src/plugin-macros.h.in  -> obs-process-monitor/src/plugin-macros.h.in (replace)
src/ProcessMonitorWidget.hpp  -> obs-process-monitor/src/  (add)
src/ProcessMonitorWidget.cpp  -> obs-process-monitor/src/  (add)
```

### 3. Generate the Visual Studio solution

```cmd
cmake --preset windows-x64
```

This will download OBS headers and Qt automatically (first run takes a few minutes).

### 4. Build

```cmd
cmake --build build_x64 --config Release
```

Or open `build_x64\obs-process-monitor.sln` in Visual Studio and build `Release`.

### 5. Install

Copy the output DLL to OBS:

```
build_x64\Release\obs-process-monitor.dll
  -> C:\Program Files\obs-studio\obs-plugins\64bit\obs-process-monitor.dll
```

Restart OBS.

---

## Usage

After installation, go to:

```
OBS menu: View -> Docks -> Process Monitor
```

Dock it wherever you like. Position is saved automatically.

---

## Metrics explained

| Metric | Source | What it means |
|--------|--------|---------------|
| CPU % | `GetProcessTimes` (kernel + user time delta) | How much CPU the OBS process is consuming |
| RAM | `GetProcessMemoryInfo` → `WorkingSetSize` | Physical memory pages currently allocated to OBS |

Bar color: **blue** (normal) → **orange** (≥70%) → **red** (≥90%)

---

## GPU note

Per-process GPU usage on Windows requires either NVAPI (NVIDIA only) or
Windows PDH `\GPU Engine` counters, which are significantly more complex.
CPU + RAM are the primary indicators of OBS performance overhead.

---

## Files

```
obs-process-monitor/
├── CMakeLists.txt
├── buildspec.json
├── README.md
└── src/
    ├── plugin-main.cpp           OBS module entry point
    ├── plugin-macros.h.in        Version macro template
    ├── ProcessMonitorWidget.hpp  Dock widget declaration
    └── ProcessMonitorWidget.cpp  Win32 CPU/RAM measurement + Qt UI
```

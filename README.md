# TuksBaja Telemetry | ESP32 Dashboard

Iteration 1 of the electrical subteam's telemetry system. An ESP32 running
ESP-IDF reads wheel speed from four Hall effect sensors and displays live
speed and RPM on an ILI9341 screen mounted on the steering wheel.

## What this does

- Reads pulses from 4x A3144 Hall effect sensors (one per wheel)
- Computes RPM per wheel using an interrupt-driven pulse count and a moving
  average filter
- Renders a live speedometer/RPM gauge on a 240x320 ILI9341 display using LVGL
- Broadcasts telemetry over a WiFi access point to a browser dashboard for
  logging and analysis
- Runs entirely on 12V vehicle power, stepped down to the ESP32's 3.3V logic

## Getting started

You need ESP-IDF v5.5.4 installed first. Follow Espressif's official guide:
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/

Every new terminal session needs the ESP-IDF environment exported before
`idf.py` commands will work.

**Mac/Linux:**
```bash
source ~/esp/v5.5.4/esp-idf/export.sh
```

**Windows (PowerShell):**
```powershell
.\esp\v5.5.4\esp-idf\export.ps1
```

If this is skipped, CMake will fail to configure the project.

### First build

```bash
idf.py set-target esp32
idf.py menuconfig   # see "Required menuconfig settings" below
idf.py build
idf.py -p <PORT> flash monitor
```

### Required menuconfig settings

These are checked into `sdkconfig.defaults`, but if your build complains
about missing fonts, verify them manually:

```
Component config → LVGL configuration → Font usage
  → enable Montserrat 24
  → enable Montserrat 48

Partition Table → Partition Table → Custom partition table CSV
  → set to "partitions.csv"
```

## Architecture

The project is split into components under `components/`, each with a single
responsibility. This keeps the codebase understandable as the team grows and
lets people work on separate parts without stepping on each other.

```
data_types    →  shared structs/enums, no logic (SensorEvent_t, WheelIndex_t)
data_queues   →  FreeRTOS queue definitions, shared between producer/consumer
wheel_speed   →  Hall sensor ISR + RPM calculation, writes to data_queues
ui            →  LVGL widgets only. No hardware, no FreeRTOS. Portable to
                  the desktop simulator with zero changes.
display       →  glue layer: reads wheel_speed data, calls ui_update(),
                  owns the FreeRTOS display task
dashboard     →  WiFi AP + web server for live telemetry / session logging
```

### Data flow

```
Hall sensor (GPIO interrupt)
    │
    ▼
wheel_speed.c  ─── ISR pushes SensorEvent_t ──▶  data_queues (FreeRTOS queue)
    │
    ▼
processing task reads queue, updates moving average per wheel
    │
    ▼
wheel_speed_get_all_rpm()  ◀── called by display task (mutex-protected)
    │
    ▼
display.c  ── calls ──▶  ui_update(speed_kmh, rpm)  ── renders ──▶  ILI9341
```

`ui.c` never touches FreeRTOS, queues, or GPIO directly. It only exposes
`ui_init()` and `ui_update(speed_kmh, rpm)`. This is deliberate — the same
file compiles against LVGL's desktop simulator (SDL2) with no changes,
which means UI work can happen on a laptop without any hardware.

## Working on the UI without hardware

LVGL has an official desktop simulator. This is how you should be doing UI
work day to day — waiting for a flash cycle to see a font size change is
not a good use of your time.

Setup:
```bash
brew install sdl2 cmake   # macOS
```

Clone the simulator (search "lvgl simulator cmake github" if this URL has
moved):
```bash
git clone --recurse-submodules https://github.com/lvgl/lv_port_pc_vscode.git
```

`components/ui/ui.c` and `include/ui.h` are the same files referenced by
both the simulator build and the ESP-IDF build. Copy them into the
simulator's source tree, iterate there, then copy back — see the team wiki
for the exact simulator project layout once we've set up the shared
submodule.

## Team conventions

- **One component, one responsibility.** If you're adding a feature and it
  doesn't obviously belong in an existing component, that's a sign it
  deserves its own.
- **`REQUIRES` in `CMakeLists.txt` should be minimal.** Only list what you
  directly `#include`. This keeps the dependency graph honest.
- **Shared constants belong in `data_types.h`**, not duplicated across
  component headers (we've hit duplicate-`#define` warnings from this
  already — check before adding a new constant).
- **`sdkconfig` is gitignored, `sdkconfig.defaults` is not.** If you change
  a menuconfig setting the whole team needs, copy the relevant
  `CONFIG_...=y` lines into `sdkconfig.defaults` so it applies on a fresh
  clone.

## Hardware (iteration 1)

| Component | Notes |
|---|---|
| ESP32 dev board | 3.3V logic |
| 4x A3144 Hall effect sensor | One per wheel, digital pulse output |
| ILI9341 240x320 TFT | SPI, steering wheel mounted |
| 12V → 5V/3.3V regulator | Powered from vehicle battery |

GPIO assignments are defined in `components/wheel_speed/wheel_speed.c` —
check there before wiring, and update the table above if pins change.

## Repo structure

```
data_acquisition/
├── README.md                (this file)
├── partitions.csv            single source of truth for flash layout
├── sdkconfig.defaults        team-shared menuconfig settings
├── CMakeLists.txt
├── components/
│   ├── data_types/
│   ├── data_queues/
│   ├── wheel_speed/
│   ├── ui/
│   ├── display/
│   └── dashboard/
└── main/
    └── main.c
```

# System Guide: How the Telemetry System Works

This is a deeper technical walkthrough of the codebase, meant to sit alongside
the top-level [README.md](../README.md). The README covers build/flash setup
and the high-level component split; this doc covers the **runtime behavior**:
what calls what, what data structures exist, and where to make changes
without breaking something else.

There are, effectively, **two independent consumers** of wheel-speed data:

1. **The physical LCD** on the steering wheel (`display` + `ui` components)
2. **The web dashboard** at `192.168.4.1` (`dashboard` component)

Both read from the same source of truth (`wheel_speed`), but they are
separate code paths that can drift out of sync from each other — that's
happened before (see [Known gaps](#known-gaps--things-to-watch-for) below),
so keep that in mind when editing one and expecting the other to change too.

---

## 1. Component map

```
components/
├── data_types/    shared structs & enums only — no logic, no includes of
│                  other components. Everything else depends on this.
├── data_queues/   owns the one FreeRTOS queue that carries sensor pulses
│                  from ISR context into task context
├── wheel_speed/   GPIO interrupts + RPM math. The single source of truth
│                  for "how fast is each wheel spinning"
├── ui/            LVGL widgets only. No FreeRTOS, no GPIO, no networking.
│                  Portable to LVGL's desktop simulator unchanged.
├── display/       glue: pulls RPM from wheel_speed, pushes it into ui,
│                  owns the SPI/ILI9341 driver setup and the display task
└── dashboard/     WiFi AP + HTTP/WebSocket server + GPS + a second,
                   separate speed-calculation path (see gap #1 below)
```

`main/main.c` is the entry point (`app_main`) that wires all of this
together at boot. If you're trying to understand "what starts first,"
start there.

---

## 2. Boot sequence (`main/main.c`)

```
app_main()
 ├─ queues_init()              create the wheelSensorQueue
 ├─ hall_sensor_init()         configure GPIO 12/13/14/27 + ISRs, start
 │                             the wheel_speed "processing" task
 ├─ display_init()             bring up SPI/ILI9341, LVGL, start
 │                             the display_task
 ├─ nvs_flash_init()           required before WiFi can start
 ├─ speed_calc_init(GPIO 19)   starts a SEPARATE pulse counter (see gap #1)
 ├─ gps_init(TX 17, RX 16)     starts the UART GPS reader task
 ├─ wifi_init_softap()         brings up the "BAJA_DASH" access point
 ├─ start_webserver()          registers HTTP routes, starts telemetry_task
 └─ while(1) { ... }           5s heartbeat log, nothing else
```

Everything after init runs as independent FreeRTOS tasks — there is no
central loop driving the system. If you add a new sensor or feature, you'll
almost always be adding a new task (via `xTaskCreate`) rather than adding to
the `while(1)` in `main.c`.

---

## 3. Data flow — physical LCD path

```
Hall sensor pulse (GPIO falls LOW)
        │  ISR: hall_isr_handler()  [wheel_speed.c]
        ▼
SensorEvent_t { sensor_id, timestamp_us }
        │  xQueueSendFromISR
        ▼
wheelSensorQueue                    [data_queues.c]
        │  xQueueReceive (in "processing" task)
        ▼
processing()                        [wheel_speed.c]
   - computes period between pulses → RPM
   - pushToMovingAverage() into wheelInfos[sensor_id]
        │
        ▼
wheel_speed_get_all_rpm(float out[4])   ◀── mutex-protected getter
        │  polled every ~20ms by display_task
        ▼
display_task()                      [display.c]
   - avg_rpm = (FL+FR+RL+RR) / 4.0f   (matches web dashboard's averaging)
   - converts to km/h
   - calls ui_update(speed_kmh, avg_rpm)
        ▼
ui_update()                         [ui.c]
   - sets LVGL arc + label values
        ▼
lv_timer_handler() → lvgl_flush_cb() → esp_lcd_panel_draw_bitmap()
        ▼
ILI9341 screen
```

## 4. Data flow — web dashboard path

```
Same 4 hall sensors → same wheel_speed_get_all_rpm()
        │  polled every 100ms by telemetry_task
        ▼
telemetry_task()                    [web_server.c]
   - avg_rpm = (FL+FR+RL+RR) / 4.0f   ← always divides by 4, see gap #3
   - speed_ms = avg_rpm * circumference / 60
        ▼
telemetry_broadcast()
   - JSON: {"speed_ms": ...}
   - pushed to every connected client over /ws
        ▼
Browser: ws.onmessage → latestFrame = JSON.parse(...)
        ▼
renderTelemetry()  (requestAnimationFrame loop)  [index.html]
   - speedKmh = latestFrame.speed_ms * 3.6
   - writes into #speedVal
```

GPS and session state ride the same WebSocket frame but aren't fully wired
up yet — see [gap #3](#known-gaps--things-to-watch-for).

---

## 5. Data structures ("classes")

Everything here is a plain C struct — there's no OOP in this codebase, but
these are the types that matter when tracing data through the system.

| Type | Defined in | Purpose |
|---|---|---|
| `WheelIndex_t` | `data_types.h` | Enum: `WHEEL_FRONT_LEFT=0, WHEEL_FRONT_RIGHT=1, WHEEL_REAR_LEFT=2, WHEEL_REAR_RIGHT=3`. Index into every `[WHEEL_COUNT]` array in the codebase. |
| `SensorEvent_t` | `data_types.h` | `{ sensor_id, timestamp_us }` — what the ISR pushes onto the queue for one pulse. |
| `MovingAverage_t` | `data_types.h` | Circular buffer (`MOVING_AVERAGE_WINDOW_SIZE = 10` samples) + running sum, used to smooth RPM. |
| `WheelInfo_t` | `data_types.h` | `{ movingAverage, lastTimestamp }` — one per wheel, lives in `wheelInfos[4]` inside `wheel_speed.c`. |
| `telemetry_frame_t` | `web_server.h` | The struct backing WebSocket broadcasts. **Note:** only `timestamp_ms` and `speed_ms` are actually populated by `telemetry_task` today — the rest (`latitude`, `lap`, `wheel_slip_l/r`, etc.) are declared but unused. |
| `session_info_t` | `web_server.h` | Declared for the session list feature; not yet populated anywhere (see gap #3). |

`WHEEL_COUNT` (4) and `MOVING_AVERAGE_WINDOW_SIZE` (10) are both defined once
in `data_types.h` — don't redefine them locally in another file.

---

## 6. FreeRTOS tasks

| Task | Created in | Priority | Cadence | Does |
|---|---|---|---|---|
| `processing` | `wheel_speed.c` | 10 | drains queue, then sleeps 50 ticks | Converts raw pulses into per-wheel moving-average RPM |
| `display_task` | `display.c` | 5 | ~20ms (50Hz) | Reads RPM, updates LVGL UI, drives the LCD flush |
| `gps_reader_task` | `gps_handler.c` | 5 | UART read w/ 100ms timeout, then 10ms delay | Parses `$GPRMC` NMEA sentences into lat/lon |
| `speed_calc_task` | `speed_calculator.c` | 5 | 1000ms | Reads a hardware pulse counter (PCNT) on GPIO 19, computes speed/odometer — see gap #1 |
| `telemetry_task` | `web_server.c` | 5 | 100ms (10Hz) | Reads all 4 wheels' RPM, averages, broadcasts over WebSocket |

The hall-sensor ISR itself (`hall_isr_handler`) doesn't run as a task — it
runs in interrupt context, does minimal work (debounce check + queue push),
and returns immediately. Never add blocking calls, logging, or mutex waits
inside it.

---

## 7. Web dashboard: HTTP + WebSocket surface

Registered in `start_webserver()` ([web_server.c](../components/dashboard/web_server.c)):

| Route | Method | Handler | Status |
|---|---|---|---|
| `/` | GET | `root_get_handler` | Serves the embedded `index.html` |
| `/data` | GET | `data_get_handler` | Returns CSV text from `speed_calculator.c` / `gps_handler.c`. **Not called by the current frontend** — dead from the browser's perspective, but still a valid endpoint if you `curl` it. |
| `/sessions` | GET | `sessions_get_handler` | Hardcoded to return `[]` — see gap #3 |
| `/track/map` | GET | `track_map_get_handler` | Hardcoded to return an empty `LineString` |
| `/ws` | GET (upgrade) | `ws_handler` | WebSocket. Server-push only — client frames are read and discarded |

The frontend also calls `POST /session/start`, `POST /session/stop`, and
`POST /track/map`, and `GET /data/replay?id=...` — **none of these are
registered** in `start_webserver()`. Clicking Start/Stop/Upload in the UI
today will just fail silently (caught by the `try/catch` in each handler).
See gap #3 if you're going to implement session logging.

### Frontend structure (`index.html`)

Single file, no build step, no external dependencies (deliberately — it's
embedded into flash via `EMBED_FILES` in `CMakeLists.txt`, so keeping it
small matters). Organized as:

- `CONFIG` object at the top of the `<script>` — WebSocket URL, base URL,
  reconnect backoff, track-simplification tuning
- `connectWebSocket()` / `scheduleReconnect()` — WS lifecycle with
  exponential backoff (caps at 5s)
- `renderTelemetry()` — a `requestAnimationFrame` loop that reads the latest
  buffered WS frame (`latestFrame`) and writes into the DOM. It does **not**
  redraw on every WS message; it redraws every animation frame and just
  reads whatever the last message was.
- `initMapCanvas()` / `drawTrackOutline()` / `drawCarMarker()` — two-layer
  canvas map (track outline + car position)
- Session manager, lap chart, and track builder sections are UI shells with
  handlers wired to endpoints that don't exist yet server-side (gap #3)
- `startTestMode()` — if you load the page from anywhere other than
  `192.168.4.1` (e.g. opening `index.html` locally, or via
  `test_dummy_data.js`), it fabricates fake telemetry at 10Hz instead of
  opening a WebSocket. Useful for frontend-only iteration without hardware.
  Toggle manually with `Ctrl+T`.

---

## 8. How to edit — common tasks

**Change a hall sensor's GPIO pin**
Edit the `#define HALL_*_GPIO` block at the top of
[`wheel_speed.c`](../components/wheel_speed/wheel_speed.c). Update the
hardware table in the README to match — nothing else needs to change,
`hall_sensor_init()` picks the new pin up automatically.

**Add a new field to the WebSocket telemetry frame**
1. Populate it in `telemetry_frame_t` usage inside `telemetry_task()`
   ([web_server.c](../components/dashboard/web_server.c)) — right now only
   `speed_ms` is filled in; extend the `snprintf` in `telemetry_broadcast()`
   to include it in the JSON.
2. Read it in `index.html`'s `ws.onmessage` / `renderTelemetry()` the same
   way `speed_ms` is read (`latestFrame.your_field`).
3. If it's also needed in `startTestMode()`, add it to the fabricated
   `frame` object there so the no-hardware test path stays in sync.

**Change how RPM is averaged/smoothed**
`MOVING_AVERAGE_WINDOW_SIZE` in `data_types.h` controls how many samples
are averaged per wheel (currently 10). Larger = smoother but laggier.
The actual math is `pushToMovingAverage()` / `readMovingAverage()` in
`wheel_speed.c`.

**Add a new HTTP endpoint**
Follow the pattern in `start_webserver()`: write an `esp_err_t
your_handler(httpd_req_t *req)`, build an `httpd_uri_t`, register it with
`httpd_register_uri_handler`. Keep `config.max_uri_handlers` (currently 10)
greater than or equal to your total route count or registration will
silently fail once you hit the cap.

**Work on the LCD UI without hardware**
`ui.c`/`ui.h` are hardware-agnostic on purpose (no FreeRTOS, no GPIO). See
the README's "Working on the UI without hardware" section for the LVGL
simulator setup — this is the fastest iteration loop for gauge/layout work.

**Work on the web dashboard without hardware**
Open `index.html` directly in a browser (not via the ESP32's IP) — it
detects it's not on `192.168.4.1` and calls `startTestMode()` automatically,
feeding itself fake telemetry. `test_dummy_data.js` in the same folder is a
related standalone script — check it if you need a non-browser data source.

---

## 9. Known gaps / things to watch for

These aren't necessarily bugs to rush and fix — some may be deliberate
work-in-progress — but they're worth knowing about before you assume
behavior that isn't actually there.

1. **`speed_calculator.c` is a second, disconnected speed source.**
   `speed_calc_init()` is initialized in `main.c` with `HALL_SENSOR_GPIO =
   19` — a GPIO that is *not* one of the four wheel-speed pins (12/13/14/27)
   and has nothing wired to it in the current hardware setup. Its output
   (`get_current_speed()`) only feeds the unused `/data` HTTP endpoint, not
   the WebSocket the dashboard actually displays. If you're chasing a speed
   discrepancy, make sure you know which of the two speed pipelines you're
   looking at.
2. **Both speed paths are always `sum / 4`, not `sum / active_count`.**
   `display_task()` (LCD) and `telemetry_task()` (web dashboard) both
   average all four wheels the same way now. If you're bench-testing with
   fewer than 4 sensors connected, the displayed speed on both will read
   roughly `n/4` of what a fully-wired car would show for the same
   per-wheel RPM. This is expected, not a bug — worth remembering so a
   "low" number during single-sensor testing doesn't look like a failure.
3. **Session logging isn't implemented server-side.** `web_server.h`
   declares `session_start()`, `session_stop()`, `session_is_active()`, and
   `session_get_list()`, and `index.html` already has buttons wired to POST
   `/session/start`, `/session/stop`, and read `/sessions` — but none of
   these are implemented or registered in `web_server.c`. `/sessions`
   currently always returns `[]`, and the POST routes 404. This is the
   biggest "not yet built" piece of the web dashboard.
5. **GPS parsing is minimal and unauthenticated against checksums.**
   `gps_reader_task()` in `gps_handler.c` does a simple `strtok` parse of
   `$GPRMC` sentences with no NMEA checksum validation — a corrupted
   sentence could silently produce a bad lat/lon. Fine for now, but don't
   assume `get_gps_latitude()`/`get_gps_longitude()` are validated.

---

## 10. Hardware notes worth remembering while editing

- Hall sensor GPIOs are configured `GPIO_INTR_NEGEDGE` with the **internal
  pull-up enabled**. They idle HIGH and the ISR fires on a falling edge
  (pin pulled toward GND) — not on the pin being driven positive. Keep this
  in mind if you ever add a new digital sensor input the same way.
- A3144 Hall sensors need **4.5–24V supply** — don't power them from the
  ESP32's 3.3V rail. Their output is open-collector, so it's safe (and
  expected) to pull that output line up to 3.3V with your own resistor even
  though the sensor itself runs at 5V.
- `HALL_SENSOR_DEBOUNCE_uS` (10ms) in `wheel_speed.c` means pulses closer
  together than that are ignored — relevant if you're bench-testing by hand
  and pulses seem to "drop."

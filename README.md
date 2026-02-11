# SmallTV Open Firmware

A clean, minimal base firmware for the **GeekMagic SmallTV Pro**. It handles the basics (WiFi setup, web-based configuration, OTA updates, a clock/weather display) so you can fork it and focus on building whatever you actually want your SmallTV to do.

## Target Hardware

This firmware is built specifically for the **GeekMagic SmallTV Pro** and its **ESP32-S3** chip. It drives the onboard ST7789V 240x240 display via SPI and uses the built-in capacitive touch sensor for input.

Other SmallTV variants (Ultra, S3, etc.) have different pin mappings and hardware, so this won't work on those without modification.

## Features

**WiFi connection management** with a two-tier approach. On first boot (or if saved credentials fail), the device creates its own WiFi access point with a captive portal. Connect to it, pick your network from the scan list, and enter your password. After that, it reconnects automatically on every boot and monitors connection health in the background. If it loses connection, it tries to reconnect a few times before falling back to AP mode again.

**Web UI for configuration.** Once connected, open the device's IP address (or `smalltv-XXXX.local` via mDNS) in a browser. From there you can adjust display brightness, set your location for weather, change temperature units (F/C), configure your timezone, scan and switch WiFi networks, upload firmware, or factory reset the device. The UI is a single-page app embedded directly in the firmware, so there's no separate file system to manage.

**Over-the-air firmware updates** in two flavors. You can upload a `.bin` file through the web UI, or use ArduinoOTA from PlatformIO/Arduino IDE over the network. Either way, the device uses a dual-partition OTA scheme with automatic rollback protection. After flashing new firmware, you have 10 minutes to hit the `/confirm-good` endpoint. If you don't (because the new firmware is broken and can't serve the web UI), the bootloader rolls back to the previous working version on the next reboot.

**Clock and weather display.** The main screen shows the current time (synced via NTP), date, and current weather conditions pulled from the Open-Meteo API every 15 minutes. Weather uses WMO codes to show conditions like Clear, Cloudy, Rain, Snow, etc. A second page (tap the screen to switch) shows system info: firmware version, WiFi status, IP, signal strength, uptime, and free heap.

**Touch input.** Tap to cycle between display pages, long-press (2 seconds) to toggle the screen on/off. The screen also auto-dims after 60 seconds of no interaction. The touch driver self-calibrates on boot and adapts to environmental drift over time.

**Boot safety.** If the firmware crashes repeatedly during startup (5 times in a row), it automatically resets all settings and reboots clean. There's also a manual factory reset: power-cycle the device 5 times quickly and it wipes everything.

**Persistent settings** stored in NVS (non-volatile storage). Brightness, temperature unit, location, timezone, hostname, and WiFi credentials all survive reboots.

## Getting Started

Flashing instructions are still being finalized. The SmallTV Pro doesn't have USB; the initial flash requires a USB-to-TTL adapter connected to the GPIO header pins. Once the firmware is on the device, all subsequent updates happen over WiFi through the web UI's OTA upload page.

This project is built with [PlatformIO](https://platformio.org/) and the Arduino framework.

## Web UI

The web interface is a single-page app served directly from the ESP32's flash memory. It provides:

- **Status panel** showing firmware version, WiFi info, signal strength, uptime, and memory usage
- **WiFi section** for scanning networks and connecting to a new one
- **Settings** for brightness (slider), temperature unit toggle (F/C), GMT offset, and lat/lon for weather
- **Actions** for uploading firmware and triggering a factory reset

There's also a separate firmware upload page at `/update` with a drag-and-drop file picker and progress bar.

The full standalone version of the web UI lives in `web-ui/index.html` for development. The version embedded in the firmware (inside `src/web_server.cpp`) is gzip-compressed and served with `Content-Encoding: gzip`.

## Project Structure

```
├── include/
│   └── config.h            # All compile-time constants (pins, timeouts, defaults)
├── src/
│   ├── main.cpp            # Setup, main loop, page rendering, screen dimming
│   ├── display.h/cpp       # LovyanGFX driver, clock/weather/AP/OTA screens
│   ├── wifi_manager.h/cpp  # STA/AP mode, captive portal, scan, reconnect logic
│   ├── web_server.h/cpp    # HTTP routes, embedded web UI, JSON API
│   ├── ota.h/cpp           # ArduinoOTA + web upload + rollback watchdog
│   ├── settings.h/cpp      # NVS-backed persistent settings + boot safety counters
│   ├── weather.h/cpp       # Open-Meteo API client, WMO code mapping
│   ├── touch.h/cpp         # Capacitive touch with self-calibration and gestures
│   └── logger.h/cpp        # Circular log buffer with serial output
├── web-ui/
│   └── index.html          # Standalone web UI (development version)
├── platformio.ini          # Build config, pin definitions, library deps
└── README.md
```

## Building on Top of This

The firmware is structured so each module is self-contained with its own `.h` and `.cpp` files. To add a new feature:

1. **Add a new module.** Create `src/myfeature.h` and `src/myfeature.cpp`. Expose an `init()` function and an `update()` function that gets called every loop iteration.

2. **Wire it into main.cpp.** Add your `#include`, call your init in `setup()`, and call your update in `loop()`.

3. **Add a display page.** If your feature needs screen real estate, add a new entry to the `DisplayPage` enum in `main.cpp` and add a case to the rendering switch. The touch tap already cycles through all pages.

4. **Add web API endpoints.** Register new routes in `webServerInit()` inside `web_server.cpp`. The existing pattern (parse JSON with ArduinoJson, respond with JSON) is straightforward to follow.

5. **Add persistent settings.** Add fields to the `Settings` struct in `settings.h`, add matching NVS keys and load/save calls in `settings.cpp`, and bump `SETTINGS_VERSION` in `config.h` so existing devices get clean defaults.

All hardware pin assignments and timing constants live in `platformio.ini` (build flags) and `include/config.h`, so you can tune things without digging through the source.

### Libraries Used

- **[LovyanGFX](https://github.com/lovyan03/LovyanGFX)** for display driving (fast SPI, PWM backlight)
- **[ArduinoJson](https://arduinojson.org/)** v7 for API request/response parsing

Both are pulled in automatically by PlatformIO from `platformio.ini`.

## License

MIT

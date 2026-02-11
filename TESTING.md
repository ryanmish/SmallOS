# SmallTV Firmware - Progressive Testing Plan

## Strategy

Test on generic ESP32 dev boards first (built-in USB, easy flash), then move to actual SmallTV Pro hardware. The dev boards use the same ESP32-WROOM-32 module, so all software-only features behave identically.

**OTA rollback approach:** Do NOT call `/confirm-good` until we've tested across multiple firmware iterations and everything works. The GeekMagic stock firmware stays on the other partition as an automatic fallback. Stock firmware is also available on GitHub if we ever need to restore via serial.

---

## Phase 1: Dev Board (No Display)

**Hardware:** Any ESP32-DevKitC or similar board with built-in USB.

LovyanGFX does not crash when no display is connected. It sends SPI commands into the void and returns success. The firmware boots and runs normally.

### What to test (in order)

- [ ] **Flash and boot**: Flash via USB. Watch serial output for full boot sequence (chip info, NVS init, boot counter, display init, touch calibration, WiFi init). Confirm no crashes.

- [ ] **AP mode + captive portal**: Connect phone to `SmallTV-XXXX` AP. Open `192.168.4.1`. Verify captive portal redirect works. Verify full web UI loads and renders correctly.

- [ ] **WiFi configuration**: Use web UI to scan networks (returns cached results from pre-AP scan). Select home network, enter password, connect. Watch serial for reboot and STA connection.

- [ ] **Web UI in STA mode**: Navigate to `http://<device-ip>/`. Verify all sections load. Test every API endpoint:
  - [ ] `/api/status` returns valid JSON
  - [ ] `/api/set?brt=50` changes brightness (check serial log)
  - [ ] `/api/set?gmt=-18000` changes timezone
  - [ ] `/api/set?tempF=0` switches to Celsius
  - [ ] `/api/location` POST with lat/lon saves location
  - [ ] `/api/weather` returns weather data (after location is set)
  - [ ] `/api/scan` returns cached WiFi networks
  - [ ] `/log` returns log buffer

- [ ] **mDNS**: After WiFi connection, try `http://smalltv-XXXX.local/` from laptop. Confirm it resolves.

- [ ] **Weather fetch**: Set valid lat/lon coordinates via web UI. Wait 10 seconds (initial fetch delay). Check serial log for successful Open-Meteo response. Check `/api/weather` endpoint.

- [ ] **OTA upload**: Go to `/update`, upload the same `.bin` file. Watch serial for upload progress, reboot, and rollback watchdog activation. Then hit `/confirm-good` and verify watchdog cancels.

- [ ] **Settings persistence**: Change brightness, temp unit, location via web UI. Reboot (power cycle). Verify settings survive reboot via `/api/status`.

- [ ] **Factory reset (web)**: Hit factory reset in web UI. Verify NVS cleared (WiFi creds gone, device boots into AP mode again, settings back to defaults).

- [ ] **Boot safety - power cycle counter**: Rapidly power cycle the device 5 times within 10 seconds. Verify power cycle counter triggers factory reset on 5th boot (serial log).

- [ ] **Boot safety - boot failure counter**: This is harder to test without a crashing firmware. Verify via serial logs that the counter increments on each boot and resets after successful boot.

- [ ] **NTP time sync**: After WiFi connection, check serial log for time sync. Verify `/api/status` shows uptime incrementing.

### What CAN'T be tested here

- Display rendering (no screen connected)
- Touch input (floating pin, garbage readings)
- Backlight PWM behavior
- Visual layout and colors

---

## Phase 2: Dev Board + External Display (Optional)

**Hardware:** ESP32-DevKitC + ST7789V 240x240 breakout board + 6 jumper wires + breadboard.

### Wiring

| Display pin | DevKitC GPIO | Build flag |
|---|---|---|
| SCK / SCLK | 18 | `TFT_SCK=18` |
| SDA / MOSI | 23 | `TFT_MOSI=23` |
| DC | 2 | `TFT_DC=2` |
| RST | 4 | `TFT_RST=4` |
| BL | 25 | `TFT_BL=25` |
| VCC | 3.3V | - |
| GND | GND | - |

**Backlight caveat:** The SmallTV Pro has an inverted backlight (LOW = bright). A generic breakout probably has standard polarity (HIGH = bright). Colors may also look inverted. These are expected differences that don't affect whether the code works.

### What to test

- [ ] Display initializes and shows content
- [ ] Clock face layout: time centered, date legible, weather in right spot
- [ ] AP mode info screen shows SSID, password, IP
- [ ] "Waiting for NTP..." message screen
- [ ] OTA progress bar renders during firmware upload
- [ ] Weather data renders in bottom half after fetch

### What still CAN'T be tested here

- Touch input (no copper pad)
- Exact backlight brightness curve (inverted polarity)
- Exact color accuracy (panel inversion may differ)

---

## Phase 3: SmallTV Pro (Real Hardware)

**Hardware:** SmallTV Pro + CP2102 USB-to-TTL adapter (soldered to UART pads).

### Pre-flash checklist

- [ ] Stock GeekMagic firmware is available on GitHub for restoration
- [ ] UART pads are soldered (GND, TXD0, RXD0, 3V3, GPIO0)
- [ ] CP2102 confirmed working (loopback tested, `/dev/cu.usbserial-0001`)

### Flash strategy

**Option A: OTA via GeekMagic's web UI (preferred, preserves rollback)**
1. Connect to SmallTV's stock AP
2. Upload custom firmware `.bin` through GeekMagic's OTA page
3. Device reboots into custom firmware with `PENDING_VERIFY` state
4. GeekMagic stays on other partition as automatic fallback
5. If custom firmware crashes -> auto-rollback to GeekMagic
6. If custom firmware works but no `/confirm-good` in 10 min -> auto-rollback

**Option B: Serial flash via UART (if OTA doesn't work)**
1. Connect CP2102
2. Hold GPIO0 low, reset device (enter download mode)
3. `pio run -t upload --upload-port /dev/cu.usbserial-0001`

### What to test

- [ ] **Display**: Colors correct (not inverted), text crisp at 240x240, dark navy background looks good
- [ ] **Brightness**: Default 25% is comfortable, not blown out
- [ ] **Touch - tap**: Cycles between clock page and system info page
- [ ] **Touch - long press**: Turns screen off. Second long press turns it back on.
- [ ] **Touch - double tap**: (currently unused, but verify it doesn't interfere)
- [ ] **Screen dimming**: Wait 60 seconds with no touch, verify screen dims to 5%
- [ ] **Screen wake**: Tap after dimming restores full brightness
- [ ] **Backlight PWM**: No visible flicker (44100 Hz PWM should be invisible)
- [ ] **Touch + WiFi**: Touch readings remain stable after WiFi scan (pause/resume working)
- [ ] **Full OTA cycle on real hardware**: Upload new firmware via `/update`, test rollback watchdog, confirm with `/confirm-good`
- [ ] **mDNS from phone**: Access device via `smalltv-XXXX.local` from phone browser

### DO NOT confirm firmware good (`/confirm-good`) until:

- [ ] All Phase 3 tests pass
- [ ] Device has been running stable for at least 30 minutes
- [ ] At least one successful OTA update cycle completed on real hardware
- [ ] No crashes or watchdog resets in serial log

---

## Hardware Compatibility Notes

| Feature | Dev Board | SmallTV Pro | Notes |
|---|---|---|---|
| CPU | ESP32-WROOM-32 | ESP32-WROOM-32 | Identical |
| WiFi | Same radio | Same radio | Identical behavior |
| Flash | 4MB | 4MB | Same |
| NVS | Same | Same | Settings persist identically |
| OTA partitions | Same | Same | Same rollback mechanism |
| Display | Not connected | ST7789V 240x240 | LovyanGFX doesn't crash without display |
| Touch (T9/GPIO32) | Floating pin | Copper pad | Garbage readings on dev board, expected |
| Backlight (GPIO25) | No LED | Inverted PWM | No visual output on dev board |
| USB | Built-in (easy flash) | No USB (requires UART soldering) | Dev board much easier to iterate on |
| GPIO2 | Onboard LED | TFT DC pin | LED will flicker on dev board, harmless |

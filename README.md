# Kia XCeed HUD

Open-source driver information display for a 2022 Kia XCeed PHEV, built for the
Waveshare ESP32-S3-Touch-LCD-4 Rev 3. It combines CAN/OBD-II monitoring, a
480x480 touch dashboard, and a touch-gated temporary-AP web UI. GPS and IMU integration
is documented for the next hardware-tested milestone.

> **Safety:** This is an experimental accessory, not an automotive instrument.
> Keep the factory cluster visible, never interact with the UI while driving,
> fuse the supply, and do not transmit on the vehicle bus until identifiers and
> timing have been validated. The default build is CAN listen-only.

## Features

- LVGL dashboard with speed, power, battery SOC, RPM, coolant, trip and GPS state
- Configurable/reorderable widgets persisted in flash
- Wi-Fi station or isolated access-point mode
- Touch-to-arm temporary WPA2 AP with a fresh 20-character password shown as a QR code
- One-client captive portal, five-minute join window, 15-minute session and motion revocation
- Physically activated, ten-minute phone-hotspot OTA maintenance mode
- Live configurable LCD/browser widget dashboard and OBD-II simulation mode
- Status, Config, Display, Frames and About web pages
- Live WebSocket telemetry, LCD preview, searchable CAN frame ring buffer and
  standard OBD-II PID decoding
- Data model and wiring plan for future ATGM336H GPS and BMI160 IMU support
- Host unit tests for authentication, OBD decoding, frame filtering and layout
- Arduino IDE sketch and PlatformIO/VS Code project

## Quick start

1. Read [docs/HARDWARE.md](docs/HARDWARE.md) and [docs/SAFETY.md](docs/SAFETY.md).
2. Install Arduino ESP32 `3.0.2+`, ESP32_Display_Panel, LVGL `8.3.x`,
   ArduinoJson `7.x`, ESPAsyncWebServer and AsyncTCP.
3. Open `firmware/KiaXCeedHUD/KiaXCeedHUD.ino` in Arduino IDE. Select
   **ESP32S3 Dev Module**, 16 MB flash, OPI PSRAM and USB CDC on boot.
4. Or open the repository in VS Code with PlatformIO and use `pio run`.
5. Connect CAN-H/CAN-L/GND to a fused OBD-II breakout (pins 6/14/4 or 5).
   Do not connect the separate CAN Pal: this board already has a TJA1051.
6. Touch **Web access**, scan the Wi-Fi QR code, and accept the captive portal.
   The temporary AP shuts down automatically and is revoked above 5 km/h.

Run host tests with `cmake -S test -B build/test && cmake --build build/test &&
ctest --test-dir build/test --output-on-failure`.

## Development LAN mode

Copy `TestModeSecrets.example.h` to `TestModeSecrets.h` and enter local Wi-Fi
credentials. This ignored file enables direct LAN access and shows the assigned
IP plus `http://kia-hud.local` on the LCD. It deliberately bypasses physical
presence authentication and must not be included in production builds.

Configure a maintenance hotspot at `/ota-config`, then press **OTA update** on
the stationary HUD. The display shows its temporary ArduinoOTA address and
random upload password. See [docs/BUILDING.md](docs/BUILDING.md#ota-maintenance).

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Hardware and wiring](docs/HARDWARE.md)
- [Web API](docs/WEB_API.md)
- [Build and release](docs/BUILDING.md)
- [Safety and vehicle discovery](docs/SAFETY.md)
- [Roadmap and recommendations](docs/ROADMAP.md)

License: MIT. Version: 0.10.0.

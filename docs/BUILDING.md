# Building and releases

## Arduino IDE

Install ESP32 core 3.0.7 or newer and the libraries pinned in `platformio.ini`.
Open `firmware/KiaXCeedHUD/KiaXCeedHUD.ino`; select ESP32S3 Dev Module, 16 MB
flash, OPI PSRAM, USB CDC enabled, and compile. Use LVGL 8.4 (not LVGL 9),
Arduino_GFX and SensorLib; the display/touch pin map is included in the sketch.
The sketch-local `partitions.csv` provides two 6.5 MB OTA application slots and
is selected automatically by Arduino IDE; PlatformIO references the same file.

## VS Code / PlatformIO

Install the PlatformIO extension, open the repository and run **Build**. Upload
and serial monitor tasks are generated from `platformio.ini`.

## Tests

The portable core needs only a C++17 compiler:

```sh
cmake -S test -B build/test
cmake --build build/test
ctest --test-dir build/test --output-on-failure
```

Releases follow semantic versioning. Update `VERSION`, README and the About page,
tag `vX.Y.Z`, attach the compiled `.bin`, its SHA-256, wiring diagram and release
notes. Never publish Wi-Fi settings, captured trips, VINs or raw personal traces.

## OTA maintenance

From an authenticated HUD session, open `/ota-config` and save a phone-hotspot
SSID and password. While stationary, press **OTA update** on the display. The HUD
connects to that hotspot and shows `kia-hud-ota.local`, its IP, and a new random
20-character ArduinoOTA password. Upload from Arduino IDE within ten minutes.
OTA is cancelled at 1 km/h or above. Keep stable vehicle power throughout.

ArduinoOTA authenticates the uploader and checks transfer integrity, but this
development release does not yet enforce cryptographically signed firmware.
Secure Boot, flash encryption, signed images, and rollback are required before
treating remote updates as production-grade.

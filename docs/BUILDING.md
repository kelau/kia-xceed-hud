# Building and releases

## Arduino IDE

Install ESP32 core 3.0.7 or newer and the libraries pinned in `platformio.ini`.
Open `firmware/KiaXCeedHUD/KiaXCeedHUD.ino`; select ESP32S3 Dev Module, 16 MB
flash, OPI PSRAM, USB CDC enabled, and compile. Use LVGL 8.4 (not LVGL 9),
Arduino_GFX and SensorLib; the display/touch pin map is included in the sketch.

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

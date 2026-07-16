# Architecture

The firmware is split into independently testable concerns. `VehicleBus` owns the
ESP32 TWAI driver. `Core.h` contains portable frame, PID, layout, telemetry and
authentication logic. `Config` persists a bounded JSON model in NVS. The main
sketch coordinates the LVGL display and async HTTP service.

CAN reception is non-blocking and stored in a 256-frame RAM ring. Only the newest
100 matching frames are returned by the API, preventing an HTTP client from
exhausting memory. Standard ISO 15765 responses (0x7E8–0x7EF, mode 01) are decoded.
Kia-specific PHEV signals require capture and validation before inclusion.

Security is presence-based: touch arms HTTP for 60 seconds and displays a random
six-digit PIN for up to two minutes. Successful login destroys the PIN and issues
an opaque 15-minute bearer token held only in browser memory. Reboot, logout, or
timeout revokes access. No status, configuration, frames or UI is returned before
authorization. This protects against casual local access, not a determined
attacker with Wi-Fi/CAN proximity; TLS is impractical without provisioning.

The web preview uses the same 480x480 coordinate model as the LCD. Layout values
are range checked in firmware. A production evolution should render both targets
from a shared widget registry and use binary WebSockets for higher frame rates.

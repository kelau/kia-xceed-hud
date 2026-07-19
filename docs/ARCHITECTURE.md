# Architecture

The firmware is split into independently testable concerns. `VehicleBus` owns the
ESP32 TWAI driver. `Core.h` contains portable frame, PID, layout, telemetry and
authentication logic. `Config` persists a bounded JSON model in NVS. The main
sketch coordinates the LVGL display and async HTTP service.

CAN reception is non-blocking and stored in a two-minute PSRAM ring. Only the
newest 100 frames are returned by the live API. Normal Frames-page requests use
a bounded per-frame-type cache containing one-second count, range, latest-value,
and change buckets instead of rescanning the raw ring. Browser notifications are
coalesced to one refresh per second with at most one list and one detail request
in flight. Real CAN receive work is capped at 3 ms per main-loop pass so LVGL has
a regular service opportunity. Standard ISO 15765 responses (0x7E8-0x7EF, mode
01) are decoded. Kia-specific PHEV signals require capture and validation before
inclusion.

Security is presence-based. A physical touch creates a temporary WPA2 AP with a
random SSID suffix and a fresh 20-character password held only in RAM. A standard
Wi-Fi QR code avoids typing the high-entropy credential. The AP permits one station
and a captive portal binds the first HTTP client IP for 15 minutes; joining must
occur within five minutes. Logout, timeout, reboot, or vehicle speed at or above
5 km/h destroys the credential and stops the AP. No valuable route is available
before the client is claimed. Plain HTTP still does not defend against an attacker
who obtains the QR credential and actively intercepts local traffic; authenticated
TLS would require device certificate provisioning and browser trust enrollment.

The web preview uses the same 480x480 coordinate model as the LCD. Layout values
are range checked in firmware. A production evolution should render both targets
from a shared widget registry. Server-Sent Events notify the browser of live CAN,
metric, performance, and configuration changes; the browser then fetches the
corresponding authenticated API resource.

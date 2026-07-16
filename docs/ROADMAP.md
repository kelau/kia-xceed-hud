# Recommended roadmap

1. Validate Rev 3 display/touch and CAN on a bench harness, then add hardware-in-
   the-loop smoke tests and a CAN replay mode using sanitized capture fixtures.
2. Add GPS parsing, BMI160 calibration, complementary filtering, trip persistence
   and graceful ignition-off shutdown.
3. Implement ambient/night brightness, over-temperature and low-voltage shutdown,
   stale-data indicators, CAN bus-off recovery and a watchdog health screen.
4. Add signed OTA updates with rollback, configuration export/import and release
   checksums. Keep OTA disabled while driving and require physical confirmation.
5. Decode verified XCeed PHEV metrics: traction-battery SOC/temperature, electric
   power/regen, engine state, energy consumption, range and charge status.
6. Add SD-card opt-in logging in a documented CSV format, privacy controls, trip
   summaries and offline map-free speed-limit alerts only when reliable.
7. Extend the implemented byte inspector, formula sandbox, PID discovery, ISO-TP,
   ECU map, CSV/PCAP and DBC tools, correlation, markers, diagnostics and range
   alerts with SD-card persistence and longer analysis windows.

Avoid cloud dependency: the HUD should boot, display and authenticate entirely
offline. Prefer a phone-provided hotspot over storing a home network password in a
vehicle. Add localization and metric/imperial units before broader distribution.

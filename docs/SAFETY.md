# Safety and vehicle discovery

The firmware starts in TWAI listen-only mode. Passive traffic cannot provide all
standard OBD PIDs because an ECU normally sends those only in response to a
tester. Enable normal mode only after bench testing; rate-limit mode 01 requests,
never transmit while updating firmware, and return to listen-only on bus errors.

Do not guess proprietary Kia signals. Capture stationary scenarios, vary one
property at a time, compare with factory/diagnostic readings, document arbitration
ID, byte order, scale, offset, units, counter/checksum and model-year evidence,
then add a regression fixture. Treat UDS service 0x2E, IO control, routines,
security access, coding and clearing DTCs as permanently out of scope.

Mount below the driver's sight line without blocking airbags or controls. Use a
glanceable high-contrast layout, automatic night dimming and a startup disclaimer.
GPS and frame logs are personal data; logging should be opt-in with retention and
one-touch deletion.

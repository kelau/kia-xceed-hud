# Hardware and wiring

## Waveshare ESP32-S3-Touch-LCD-4 Rev 3

The board already includes an ST7701 RGB LCD, GT911 touch controller, TCA9554 I/O
expander and TJA1051 CAN transceiver. Vendor Rev 2/3 examples assign TWAI RX to
GPIO0 and TX to GPIO6. Use the onboard CAN terminal and leave its 120-ohm switch
**off** in a vehicle; the car bus is already terminated at both ends.

OBD-II breakout: pin 6 → CAN-H, pin 14 → CAN-L, and pin 4 or 5 → GND. Power the
HUD through a fused, ignition-switched automotive 12 V supply appropriate for the
board's wide-input terminal. Do not feed OBD pin 16 directly into USB or 5 V.

## Optional modules

- ATGM336H: connect TX to a free ESP32 UART RX, GND common and use the module's
  specified supply voltage. Configure 9600 8N1. Mount with sky view. The chosen
  pins must be verified against the Rev 3 schematic before assembly.
- GY-BMI160: connect to the exposed I2C connector (board bus GPIO7 SCL / GPIO15
  SDA per Rev 2/3 documentation), respecting 3.3 V levels. Calibrate in the final
  enclosure and make the axes configurable.
- Adafruit CAN Pal: not required and must not be connected in parallel with the
  onboard TJA1051.

Use locking connectors, strain relief, a flame-retardant enclosure, reverse and
load-dump protection, and an automotive-grade fuse close to the source.

# Hardware and wiring

## Waveshare ESP32-S3-Touch-LCD-4 Rev 3

The board already includes an ST7701 RGB LCD, GT911 touch controller, TCA9554 I/O
expander and TJA1051 CAN transceiver. Vendor Rev 2/3 examples assign TWAI RX to
GPIO0 and TX to GPIO6. Use the onboard CAN terminal and leave its 120-ohm switch
**off** in a vehicle; the car bus is already terminated at both ends.

OBD-II breakout: pin 6 → CAN-H, pin 14 → CAN-L, and pin 4 or 5 → GND. Power the
HUD through a fused, ignition-switched automotive 12 V supply appropriate for the
board's wide-input terminal. Do not feed OBD pin 16 directly into USB or 5 V.

## External modules

### Waveshare LC76G GNSS Module

The LC76G(AB) replaces the planned ATGM336H and UART bridge. UART is not used.
Connect it only through the board's exposed I2C connector:

| LC76G | ESP32-S3-Touch-LCD-4 Rev 3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO15 / exposed SDA |
| SCL | GPIO7 / exposed SCL |
| TX, RX, PPS, RST | Not connected |

Use the supplied external active antenna and place it near the windscreen with
the best practical sky view, separated from the LCD ribbon, Wi-Fi antenna, CAN
wiring and switching regulators. Power from 3V3 keeps every exposed signal in
the ESP32's 3.3 V domain even though the Waveshare carrier also accepts 5 V.

The LC76G, GT911 touch controller, BMI160 and board-service devices share the
same 400 kHz bus. Do not add external I2C pull-ups unless measurements show they
are required; parallel carrier-board pull-ups can make the effective resistance
too low. Keep branch wiring short. The firmware uses the Quectel buffer protocol
at addresses 0x50 (command) and 0x54 (read), reads at most 96 bytes per
transaction, and observes the specified 10 ms command-processing interval
without blocking the display loop.

### GY-BMI160

Connect it to the same exposed I2C connector (GPIO7 SCL / GPIO15 SDA), respecting
3.3 V levels. Calibrate in the final enclosure and make the axes configurable.

### CAN Pal

The Adafruit CAN Pal is not required and must not be connected in parallel with
the onboard TJA1051.

Use locking connectors, strain relief, a flame-retardant enclosure, reverse and
load-dump protection, and an automotive-grade fuse close to the source.

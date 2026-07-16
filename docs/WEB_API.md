# Web API

The server listens on HTTP port 80 only through a touch-armed temporary AP. Scan
the displayed Wi-Fi QR code; the captive portal's first client claims the session.

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/status` | System, network and current vehicle metrics |
| GET/PUT | `/api/config` | Read or update validated persistent configuration |
| GET | `/api/frames?q=` | Up to 100 captured frames filtered by ID/hex bytes |
| GET | `/api/frame-types` | Stable aggregation by CAN ID and OBD-II PID |
| GET | `/api/frame-detail?key=` | Frequency, ranges, latest bytes and value history |
| GET | `/api/pids` | SAE PID catalog with observed and ECU-supported flags |
| GET | `/api/ecus` | Responding ECU inventory and observed PIDs |
| GET | `/api/isotp` | Reassembled buffered ISO-TP multi-frame payloads |
| GET | `/api/diagnostics` | Buffered DTC and freeze-frame responses |
| GET | `/api/export.csv` | Download captured frames and decodes as CSV |
| POST | `/api/ota-start` | Start the stationary-only OTA maintenance window |
| POST | `/api/logout` | Revoke the current device session |

Protected calls must originate from the claimed client IP during the 15-minute
session. Temporary WPA credentials exist only in RAM and must never be logged.
Stored station credentials are accepted on write but never emitted by the config
serializer.

# Web API

The server listens on HTTP port 80 only through a touch-armed temporary AP. Scan
the displayed Wi-Fi QR code; the captive portal's first client claims the session.

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/status` | System, network and current vehicle metrics |
| GET/PUT | `/api/config` | Read or update validated persistent configuration |
| GET | `/api/frames?q=` | Up to 100 captured frames filtered by ID/hex bytes |
| POST | `/api/logout` | Revoke the current device session |

Protected calls must originate from the claimed client IP during the 15-minute
session. Temporary WPA credentials exist only in RAM and must never be logged.
Stored station credentials are accepted on write but never emitted by the config
serializer.

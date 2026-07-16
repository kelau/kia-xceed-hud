# Web API

The server listens on HTTP port 80 on the local network. Before any route is
available, touch **Web access**. `GET /` then supplies the login shell.

| Method | Route | Purpose |
|---|---|---|
| POST | `/api/login` | Exchange `{ "pin": 123456 }` for a bearer token |
| GET | `/api/status` | System, network and current vehicle metrics |
| GET/PUT | `/api/config` | Read or update validated persistent configuration |
| GET | `/api/frames?q=` | Up to 100 captured frames filtered by ID/hex bytes |
| POST | `/api/logout` | Revoke the current device session |

Protected calls require `Authorization: Bearer <token>`. Tokens expire after 15
minutes. PINs and tokens must never be logged. Wi-Fi credentials are accepted on
write but never emitted by the config serializer.

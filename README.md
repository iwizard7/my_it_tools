# ESP32 IT Tools + Debian Gateway

DevOps toolkit with two complementary projects. Version `1.9.0`:

```text
esp32/             # автономная прошивка ESP32-C3 и локальный web UI
debian-gateway/    # полноценный HTTP/HTTPS gateway для Postman-like режима
```

## ESP32

```bash
cd esp32
pio run
pio run -t upload --upload-port /dev/ttyACM0
```

Connect to `ESP32-Random-Tools` with password `randomtools` and open `http://192.168.4.1`.

ESP32 provides offline developer tools, network diagnostics, Incident Probe, metrics, QR generator and a lightweight API console.

## Debian Gateway

```bash
cd debian-gateway
docker compose up -d --build
curl http://127.0.0.1:8080/healthz
```

The gateway adds HTTP/HTTPS requests, headers, query parameters, JSON/raw body, redirects, timeouts, response limits and persistent collections. See [`debian-gateway/README.md`](debian-gateway/README.md).

## Development checks

```bash
node tests/test_tools.js
pio run -d esp32
python3 -m pytest debian-gateway/tests
```

The full documentation and changelog remain in the repository root. This project is not affiliated with the original IT-Tools project.

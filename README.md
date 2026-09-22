# ESP32 IT Tools + Debian Gateway

Portable DevOps toolkit with two complementary projects. Current installer/release line: **1.9.2**.

```text
esp32/             # ESP32-C3 firmware and local web UI
debian-gateway/    # HTTP/HTTPS, DNS and traceroute backend
deploy/            # Raspberry Pi deployment helpers
install.sh         # interactive cross-platform installer
```

## One-command installation

The installer detects the host OS and architecture automatically:

- macOS;
- Debian/Ubuntu;
- Raspberry Pi OS;
- x86_64, ARM64 and ARMv7.

Run it in a terminal:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/install.sh | bash
```

Without flags, the installer asks:

1. whether to install/build the ESP32 PlatformIO toolchain;
2. whether to flash a connected ESP32;
3. whether to use Docker instead of native Python Gateway.

The default Gateway mode is a native Python virtualenv. Docker is optional.

### Installer modes

Install and flash ESP32:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/install.sh | bash -s -- --esp32
```

Install only the Gateway:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/install.sh | bash -s -- --gateway-only
```

Use Docker instead of native Python:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/install.sh | bash -s -- --docker
```

Run unattended on a server or CI host:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/install.sh | bash -s -- --non-interactive
```

Non-interactive defaults are native Gateway, no ESP32 flash and no Docker installation.

## ESP32-C3

The ESP32 creates a local Wi‑Fi network:

```text
SSID:      ESP32-Random-Tools
Password:  randomtools
URL:       http://192.168.4.1
```

Manual build and upload:

```bash
cd esp32
pio run
pio run -t upload --upload-port /dev/ttyACM0
```

On macOS the port is usually `/dev/cu.usbmodem1101`.

The firmware provides:

- random data, UUID, token/password and QR generators;
- Base64, URL, JSON and text tools;
- Cyrillic/Latin script analyzer with colored highlighting;
- DevOps Toolkit for curl, Git, Docker, Kubernetes, Linux and CI/CD helpers;
- Network Diagnostics;
- DNS Inspector UI connected to Debian Gateway;
- Incident Probe;
- Log Analyzer with local files up to 2 MB;
- API Console;
- metrics, Prometheus endpoint and OTA updates.

## Debian Gateway

The Gateway provides the heavier backend functionality:

- HTTP/HTTPS requests;
- headers, query parameters and JSON/raw bodies;
- redirects and timeouts;
- response size limit;
- persistent collections;
- DNS record lookup;
- reverse DNS;
- resolver comparison;
- DNS health checks;
- bounded traceroute through the host OS.

### Native Python mode

```bash
cd debian-gateway
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8080
```

### Docker mode

```bash
cd debian-gateway
docker compose up -d --build
curl http://127.0.0.1:8080/healthz
```

### Useful endpoints

```text
GET  /healthz
GET  /version
POST /api/request
GET  /api/collections
POST /api/collections
GET  /api/dns/lookup
GET  /api/dns/reverse
GET  /api/dns/compare
GET  /api/dns/health
GET  /api/traceroute
```

Configure the Gateway URL in the ESP32 UI under **Network → DNS Inspector**. Example:

```text
http://192.168.1.50:8080
```

## Raspberry Pi 3B and newer

The Gateway works on Raspberry Pi 3B, 3B+, 4 and 5 with Raspberry Pi OS/Debian. Native Python mode is the default, so Docker is not required:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/deploy/raspberry-pi/install.sh -o install.sh
chmod +x install.sh
./install.sh
```

The Raspberry Pi installer delegates to the universal installer and uses native Python by default. Docker is available with:

```bash
./install.sh --docker
```

See [`deploy/raspberry-pi/README.md`](deploy/raspberry-pi/README.md) for service operations and ARM details.

## Development checks

```bash
node tests/test_tools.js
pio run -d esp32
python3 -m py_compile debian-gateway/app/main.py
python3 -m pytest debian-gateway/tests
```

GitHub Actions runs the JavaScript tests, Gateway tests and PlatformIO build. Tags matching `v*` publish a firmware release.

## Project files

- `CHANGELOG.md` — release history;
- `openapi.yaml` — API reference;
- `LICENSE` — MIT license;
- `release/` — source and firmware archives;
- `.github/` — CI, release and issue templates.

This is an independent project inspired by the idea of a local IT tools collection and is not affiliated with the original IT‑Tools project.

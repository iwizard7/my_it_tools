# Raspberry Pi deployment

The Debian Gateway is compatible with Raspberry Pi 3B and newer models. The Pi becomes the execution backend for the ESP32 UI: HTTP/HTTPS requests, DNS Inspector, traceroute and persistent collections run on the Pi.

## Compatibility

| Device | Native Python | Docker | Recommendation |
|---|---:|---:|---|
| Raspberry Pi 3B / 3B+ 32-bit OS | Yes | Yes, ARMv7 | Native mode uses less memory |
| Raspberry Pi 3B / 3B+ 64-bit OS | Yes | Yes, ARM64 | Prefer native mode on 1 GB RAM |
| Raspberry Pi 4 | Yes | Yes, ARM64/ARMv7 | 64-bit Raspberry Pi OS recommended |
| Raspberry Pi 5 | Yes | Yes, ARM64 | 64-bit Raspberry Pi OS recommended |

The Gateway is a regular Python/FastAPI application and has no Raspberry Pi 3-specific CPU instructions. The Docker Compose file intentionally does not hard-code an architecture: Docker selects the native `arm/v7` or `arm64` image for the installed OS.

## Requirements

- Raspberry Pi 3B or newer;
- Raspberry Pi OS or Debian;
- network connection to the ESP32 and target services;
- at least 2 GB free storage;
- SSH access or local terminal.

Native Python mode is the default and does not require Docker.

## Automated installation

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/deploy/raspberry-pi/install.sh -o install.sh
chmod +x install.sh
./install.sh
```

The script delegates to the universal installer, detects the Pi architecture and starts the Gateway in a Python virtualenv.

## Docker installation

Use Docker only when you want container isolation:

```bash
./install.sh --docker
```

The installer uses Docker's official installer, enables Docker and starts the architecture-native Compose deployment.

## Manual native installation

```bash
git clone https://github.com/iwizard7/my_it_tools.git
cd my_it_tools
./install.sh --gateway-only
```

## Manual Docker installation

```bash
git clone https://github.com/iwizard7/my_it_tools.git
cd my_it_tools/debian-gateway
docker compose -f docker-compose.rpi.yml up -d --build
```

Verify:

```bash
curl http://127.0.0.1:8080/healthz
curl http://127.0.0.1:8080/version
```

From another machine use:

```text
http://RASPBERRY_PI_IP:8080
```

Configure the same URL in **ESP32 → Network → DNS Inspector → Gateway URL**.

## Service management

Native mode:

```bash
tail -f ~/my_it_tools/debian-gateway/gateway.log
kill "$(cat ~/my_it_tools/.gateway.pid)"
```

Docker mode:

```bash
docker compose -f docker-compose.rpi.yml ps
docker compose -f docker-compose.rpi.yml logs -f gateway
docker compose -f docker-compose.rpi.yml restart
docker compose -f docker-compose.rpi.yml down
```

Collections persist in `debian-gateway/data` in native mode or the `gateway-data` Docker volume in container mode.

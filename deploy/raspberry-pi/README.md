# Raspberry Pi 3B deployment

This deployment runs the Debian Gateway on Raspberry Pi 3B with Raspberry Pi OS/Debian. The Pi becomes the execution backend for the ESP32 UI: HTTP/HTTPS requests, DNS Inspector, traceroute and persistent collections run on the Pi. The installer uses Docker's official installation script and then verifies Docker Compose.

## Requirements

- Raspberry Pi 3B;
- Raspberry Pi OS or Debian;
- network connection to the ESP32 and target services;
- at least 2 GB free storage;
- SSH access or local terminal;
- Docker Engine and Compose plugin (installed by the script).

## Automated installation

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/deploy/raspberry-pi/install.sh -o install.sh
chmod +x install.sh
./install.sh
```

The script installs Docker, clones the repository and starts `debian-gateway/docker-compose.rpi.yml`.

## Manual installation

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

From another machine use the Pi address:

```text
http://RASPBERRY_PI_IP:8080
```

Configure the same URL in the ESP32 **DNS Inspector → Gateway URL** field. For example:

```text
http://192.168.1.50:8080
```

## Service management

```bash
docker compose -f docker-compose.rpi.yml ps
docker compose -f docker-compose.rpi.yml logs -f gateway
docker compose -f docker-compose.rpi.yml restart
docker compose -f docker-compose.rpi.yml down
```

Collections persist in the Docker volume `gateway-data`.

## Architecture

```text
Mac/phone browser
       │
       ├── ESP32 UI and local probes
       │          │
       │          └── HTTP/DNS requests to Pi
       │
       └── Raspberry Pi 3B :8080
                    ├── Postman-like HTTP Gateway
                    ├── DNS Inspector
                    ├── traceroute
                    └── persistent collections
```

The Raspberry Pi deployment uses `linux/arm/v7`, suitable for the common 32-bit Raspberry Pi OS on Pi 3B. For a 64-bit OS remove the `platform: linux/arm/v7` line or change it to `linux/arm64`.

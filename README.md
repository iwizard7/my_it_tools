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

The ESP32 **Log Analyzer** can load local `.log`, `.txt`, `.json`, `.ndjson` and `.out` files up to 2 MB. Files are processed only in the browser and are not sent to the controller or gateway.

Network Diagnostics now includes route/DHCP details, DNS details and batch TCP port checks. Real hop-by-hop traceroute is exposed by Debian Gateway at `/api/traceroute`, because raw ICMP tracing belongs on the Debian host rather than inside the ESP32 firmware.

The separate **DNS Inspector** uses Debian Gateway for advanced DNS operations: record lookup, reverse DNS, resolver comparison and DNS health checks. Configure the gateway URL in the DNS Inspector panel.

## Raspberry Pi 3B deployment

The Debian Gateway can run on Raspberry Pi 3B with Raspberry Pi OS/Debian:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/deploy/raspberry-pi/install.sh -o install.sh
chmod +x install.sh
./install.sh
```

Для всех поддерживаемых host-платформ также доступен единый установщик из корня репозитория:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/install.sh | bash
```

Он определяет macOS, Debian/Ubuntu или Raspberry Pi, устанавливает нужные зависимости и запускает Gateway в обычном Python virtualenv — Docker не обязателен. Для прошивки подключённого ESP32 используйте `--esp32`.

Если запустить установщик без ключей в обычном терминале, он сам задаст вопросы: устанавливать ли ESP32 toolchain, прошивать ли найденный USB-контроллер и использовать ли Docker. На сервере без TTY применяются безопасные значения по умолчанию: native Gateway, без прошивки ESP32.

Если нужен Docker-вариант:

```bash
curl -fsSL https://raw.githubusercontent.com/iwizard7/my_it_tools/main/install.sh | bash -s -- --docker
```

See [`deploy/raspberry-pi/README.md`](deploy/raspberry-pi/README.md) for manual installation, service management and ESP32 Gateway URL configuration.

The Transform menu includes **Cyrillic & Latin analyzer**. It highlights Cyrillic characters in red and Latin characters in blue, supports texts up to 1 MB and processes everything locally in the browser.

## Development checks

```bash
node tests/test_tools.js
pio run -d esp32
python3 -m pytest debian-gateway/tests
```

The full documentation and changelog remain in the repository root. This project is not affiliated with the original IT-Tools project.

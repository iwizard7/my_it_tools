# ESP32 IT Tools

Локальный набор developer-инструментов в стиле [IT‑Tools](https://it-tools.tech), работающий непосредственно на **ESP32-C3**. Контроллер создаёт собственную Wi‑Fi сеть и отдаёт веб-интерфейс без обязательного подключения к интернету.

> Это самостоятельная ESP32-реализация с похожей идеей и интерфейсом, а не копия исходного Vue-приложения IT‑Tools.

## Возможности

### Генераторы

- random data с аппаратной случайностью ESP32;
- UUID v4;
- token и password;
- MAC address;
- random port;
- IPv6 ULA;
- ULID;
- NanoID;
- Lorem Ipsum;
- fake test data;
- SVG placeholder;
- cron templates;
- QR code generator (SVG, до 2000 символов).
- DevOps Incident & Network Probe dashboard;
- gateway, DNS, TCP и HTTP incident checks;
- JSON incident reports с копированием и скачиванием;
- SLA/error budget calculator;
- secret scanner;
- SemVer comparator;
- Conventional Commit validator;
- Docker image parser;
- Kubernetes resource quantity converter;
- deployment checklist;
- Prometheus endpoint `/metrics`.

DevOps-раздел также содержит **Mini API Console** и **Log Analyzer**. API Console выполняет HTTP-запрос через сеть ESP32 и показывает status, IP, latency и response. Log Analyzer считает ошибки/warnings/info и умеет маскировать секреты.

Раздел **DevOps Toolkit** также содержит локальные helpers для:

- curl/API и HTTP headers;
- JSON validation;
- changelog, branch и Conventional Commit;
- Docker image, Compose, ports и healthcheck;
- Kubernetes Deployment, Service, probes, resources и kubectl;
- cron, systemd, chmod, `.env`, shell quoting и SSH config;
- PromQL, SLO, Apdex и Prometheus alerts;
- JSON logs, secret masking, GitHub Actions, artifact manifest и postmortem.

Сетевые инструменты вынесены в отдельный раздел **Network diagnostics**: DNS lookup, reachability/latency, HTTP status, TCP port check, DHCP information, mDNS service browser, Wi‑Fi scan и IPv4 subnet calculator.

### Кодирование и текст

- Base64 encoder/decoder;
- URL encoder/decoder;
- HTML entities;
- Unicode converter;
- binary converter;
- hexadecimal converter;
- case converter;
- slug generator;
- text statistics;
- simple text diff;
- JSON formatter/minifier;
- JSON array ↔ CSV.

### Безопасность и сеть

- SHA-256 hash на самом ESP32;
- HMAC helper;
- password strength analyzer;
- HS256 JWT signature verification;
- SHA-256 hash файлов в браузере;
- JWT decoder;
- URL parser;
- IPv4 subnet calculator;
- Wi‑Fi scanner;
- DNS lookup;
- HTTP status checker;
- TCP port checker;
- DHCP information;
- mDNS service browser;
- системная информация устройства;
- аппаратная генерация случайных данных.

### Возможности устройства

- собственная Wi‑Fi точка доступа;
- одновременное подключение к домашнему Wi‑Fi;
- сохранение Wi‑Fi настроек во flash;
- mDNS: `http://esp32-it-tools.local`;
- REST API для генерации, hash, system info и Wi‑Fi scan;
- OTA-обновление через веб-интерфейс;
- два OTA-слота приложения;
- базовые HTTP security headers;
- uptime, свободная RAM, flash, CPU и IP-адреса в dashboard;
- график температуры чипа, heap, uptime, generation rate, LittleFS и Wi‑Fi uptime.

## Быстрый старт

После прошивки подключитесь к Wi‑Fi сети:

```text
SSID:     ESP32-Random-Tools
Password: randomtools
URL:      http://192.168.4.1
```

Интернет для работы инструментов не требуется. После настройки домашней сети интерфейс также доступен по mDNS-адресу `http://esp32-it-tools.local` и по IP-адресу из раздела **Device & OTA**.

## Сборка на Debian/Ubuntu

Требуется Python 3 и PlatformIO:

```bash
python3 -m pip install --user platformio
pio run
```

Если `pio` не добавлен в `PATH`:

```bash
python3 -m platformio run
```

Проект автоматически скачает Espressif32 platform, Arduino framework и зависимости при первой сборке.

## Прошивка через USB

Найдите последовательный порт:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

Загрузите прошивку:

```bash
pio run -t upload --upload-port /dev/ttyACM0
```

Порт может называться `/dev/ttyUSB0` или иначе. Для доступа к USB-порту на Debian пользователю может потребоваться группа `dialout`:

```bash
sudo usermod -aG dialout "$USER"
```

После этого нужно войти в систему заново.

## Serial Monitor

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

При запуске ESP32 выводит AP IP, домашний Wi‑Fi IP (если подключён) и mDNS-адрес.

## OTA обновление

Соберите прошивку:

```bash
pio run
```

Откройте раздел **Device & OTA**, выберите файл:

```text
.pio/build/esp32-c3-devkitm-1/firmware.bin
```

и нажмите **Upload firmware**. После успешной загрузки ESP32 перезапустится. Используются два OTA-слота приложения, поэтому обновление не должно затирать рабочий слот.

## REST API

```text
GET  /api/generate?length=32&count=5&upper=true&lower=true&digits=true&symbols=true
GET  /api/system
GET  /api/wifi/scan
GET  /api/qr?text=...
GET  /api/metrics
GET  /api/incident?host=example.com&port=80&path=/health
GET  /api/http?method=GET&host=example.com&port=80&path=/
GET  /metrics
GET  /api/net?op=dns&host=example.com
GET  /api/net?op=ping&host=example.com
GET  /api/net?op=http&host=example.com
GET  /api/net?op=tcp&host=example.com&port=443
GET  /api/net?op=dhcp
GET  /api/net?op=mdns&host=http
POST /api/hash              # raw text body, SHA-256 response
POST /api/wifi              # ssid и password в form-urlencoded
POST /api/update             # firmware.bin
```

Пример:

```bash
curl http://192.168.4.1/api/system
printf 'hello' | curl -X POST --data-binary @- http://192.168.4.1/api/hash
curl 'http://192.168.4.1/api/generate?length=16&count=2&upper=true&lower=true&digits=true&symbols=false'
```

## Структура проекта

```text
platformio.ini       # PlatformIO и настройки платы
partitions.csv       # два OTA-слота для 4 MB flash
src/main.cpp         # прошивка, REST API и встроенный веб-интерфейс
README.md            # документация
CHANGELOG.md         # история изменений
openapi.yaml         # описание REST API
tests/               # тесты JavaScript-инструментов
Dockerfile           # воспроизводимая сборка на Debian
.github/             # CI, releases и issue templates
release/             # переносимые архивы исходников и firmware
```

Каталог `.pio` создаётся PlatformIO автоматически и не нужен для переноса. Он исключён из Git.

## Ресурсы

Текущая сборка использует примерно:

```text
RAM:   12.7% — 41 468 байт из 327 680
Flash: 45.8% — 900 618 байт из 1 966 080 OTA-слота
```

Большинство преобразований выполняется локально в браузере и не отправляет текст наружу. SHA-256, QR, network diagnostics, Wi‑Fi scan и аппаратная случайность выполняются на ESP32.

Полный BIP39 dictionary, RSA key generator, bcrypt и полноценные XML/YAML parser не включены в базовую сборку: они требуют дополнительных словарей, библиотек или заметного объёма flash/RAM. Их можно вынести в опциональные модули или на Debian-сервер.

## Проверки и релизы

Локальные тесты:

```bash
node tests/test_tools.js
pio run
```

GitHub Actions автоматически запускает JavaScript-тесты и PlatformIO build. Push тега вида `v1.3.0` создаёт GitHub Release с готовым firmware-архивом:

```bash
git tag v1.3.0
git push origin v1.3.0
```

Docker-сборка для Debian:

```bash
docker build -t esp32-it-tools-builder .
docker run --rm esp32-it-tools-builder
```

API описан в `openapi.yaml`. Лицензия проекта — MIT.

## Переносимые архивы

```text
release/esp32-it-tools-source.tar.gz
release/esp32-it-tools-firmware.tar.gz
```

Исходный архив позволяет полностью пересобрать проект на Debian. Firmware-архив содержит готовые `firmware.bin`, `bootloader.bin` и `partitions.bin`.

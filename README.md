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
- cron templates.

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
- JWT decoder;
- URL parser;
- IPv4 subnet calculator;
- Wi‑Fi scanner;
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
- uptime, свободная RAM, flash, CPU и IP-адреса в dashboard.

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
release/             # переносимые архивы исходников и firmware
```

Каталог `.pio` создаётся PlatformIO автоматически и не нужен для переноса. Он исключён из Git.

## Ресурсы

Текущая сборка использует примерно:

```text
RAM:   12.6% — 41 396 байт из 327 680
Flash: 41.9% — 823 552 байт из 1 966 080 OTA-слота
```

Большинство преобразований выполняется локально в браузере и не отправляет текст наружу. SHA-256, Wi‑Fi scan и аппаратная случайность выполняются на ESP32.

Полный BIP39 dictionary, RSA key generator, bcrypt, полноценные XML/YAML parser и QR encoder не включены в базовую сборку: они требуют дополнительных словарей, библиотек или заметного объёма flash/RAM. Их можно вынести в опциональные модули или на Debian-сервер.

## Переносимые архивы

```text
release/esp32-it-tools-source.tar.gz
release/esp32-it-tools-firmware.tar.gz
```

Исходный архив позволяет полностью пересобрать проект на Debian. Firmware-архив содержит готовые `firmware.bin`, `bootloader.bin` и `partitions.bin`.

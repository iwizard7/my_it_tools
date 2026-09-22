# Debian Gateway

Gateway для полноценного Postman-подобного режима ESP32 IT Tools. Debian Gateway выполняет HTTP/HTTPS-запросы, хранит коллекции и принимает большие response body, а ESP32 остаётся лёгким UI и локальным network probe.

## Запуск через Docker

```bash
docker compose up -d --build
curl http://127.0.0.1:8080/healthz
```

## Запуск напрямую

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8080
```

## API request

```bash
curl -X POST http://127.0.0.1:8080/api/request \
  -H 'content-type: application/json' \
  -d '{"method":"GET","url":"https://httpbin.org/get","params":{"source":"esp32"}}'
```

Поддерживаются HTTP methods, headers, query parameters, JSON/raw body, redirects, timeout и ограничение response до 2 MB.

Расширенный traceroute выполняется Debian Gateway, а не ESP32:

```bash
curl 'http://127.0.0.1:8080/api/traceroute?host=example.com&max_hops=16'
```

Установите утилиту на Debian:

```bash
sudo apt-get install traceroute
```

Коллекции сохраняются в volume в `data/collections.json`.

## DNS Inspector API

```bash
curl 'http://127.0.0.1:8080/api/dns/lookup?name=example.com&record_type=A'
curl 'http://127.0.0.1:8080/api/dns/reverse?name=8.8.8.8'
curl 'http://127.0.0.1:8080/api/dns/compare?name=example.com&record_type=A'
curl 'http://127.0.0.1:8080/api/dns/health?name=example.com'
```

Supported record types include A, AAAA, CNAME, MX, NS, TXT, SOA, SRV, CAA, PTR and NAPTR.

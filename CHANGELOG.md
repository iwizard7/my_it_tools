# Changelog

## 1.9.1 - 2026-09-23

- Added Raspberry Pi 3B/Raspberry Pi OS deployment for Debian Gateway.
- Added ARMv7 Docker Compose configuration, healthcheck and persistent data volume.
- Added automated installation script and Raspberry Pi operations documentation.
- Added traceroute and ping packages to the Gateway image.

## 1.8.3 - 2026-09-23

- Added Cyrillic and Latin script analyzer with colored character highlighting.
- Added contextual help entry for the new analyzer.

## 1.8.2 - 2026-09-23

- Added extended route, DNS details and batch port diagnostics to Network Diagnostics.
- Added bounded Debian Gateway traceroute endpoint for real hop-by-hop tracing.
- Kept raw traceroute out of ESP32 firmware because it requires host-level ICMP/raw socket capabilities.
- Added contextual interactive help for every ESP32 UI section.
- Added DNS Inspector for record lookup, reverse DNS, resolver comparison and DNS health checks.

## 1.8.1 - 2026-09-23

- Added local log file upload to Log Analyzer.
- Added support for `.log`, `.txt`, `.json`, `.ndjson` and `.out` files up to 2 MB.
- Files are read in the browser and never uploaded to ESP32 or Debian Gateway.

## 1.9.0 - 2026-09-22

- Split the repository into independent `esp32/` and `debian-gateway/` projects.
- Added a FastAPI Debian Gateway for Postman-like HTTP/HTTPS requests.
- Added request headers, query parameters, JSON/raw body, redirects, timeouts and response limits.
- Added persistent collection storage and Docker Compose deployment for the gateway.
- Updated CI and firmware release paths for the new project layout.

## 1.8.0 - 2026-09-22

- Added a mini Postman-style HTTP API console through the ESP32 network.
- Added browser-side log analyzer with error/warning counts and secret masking.
- Added dedicated DevOps navigation for API, logs, incident probing and metrics.
- Exposed Docker, Kubernetes, SSH and Linux helper generators through the DevOps Toolkit.

## 1.7.1 - 2026-09-22

- Fixed a JavaScript syntax error in the curl command builder that prevented the UI from initializing.

## 1.7.0 - 2026-09-22

- Added the DevOps Toolkit menu with local browser-side tools for API, Linux, Git, Docker, Kubernetes, CI/CD, secrets, observability and incident response.
- Added curl/API, HTTP headers, JSON validation, changelog, branch, Compose, Docker healthcheck, Kubernetes manifest, systemd, chmod, `.env`, shell quoting and SSH config helpers.
- Added PromQL, SLO, Apdex, Prometheus alert, log formatting/masking, GitHub Actions, artifact manifest and postmortem generators.

## 1.6.1 - 2026-09-22

- Moved DNS, IP, TCP, HTTP, DHCP, mDNS and Wi‑Fi tools into a dedicated Network Diagnostics menu.
- Added quick network operation selection and a focused diagnostic output panel.

## 1.6.0 - 2026-09-22

- Reorganized the UI into Generate, Transform, Security and DevOps workflows.
- Added SLA/error budget calculator, secret scanner, SemVer comparator, Conventional Commit validator, Docker image parser, Kubernetes quantity converter and deployment checklist.
- Added an Incident Probe workflow with gateway, DNS, TCP and HTTP checks.

## 1.5.0 - 2026-09-22

- Added DevOps Incident & Network Probe dashboard.
- Added combined gateway, DNS, TCP and HTTP incident checks.
- Added JSON incident reports with copy and download actions.
- Added Prometheus-compatible `/metrics` endpoint.
- Added live system metrics for dashboards and monitoring.

## 1.4.0 - 2026-09-22

- Added password strength analyzer and HS256 JWT verification.
- Added file SHA-256 calculation in the browser.
- Added DNS lookup, HTTP status check, TCP port check and DHCP information API.
- Added mDNS service discovery and Wi-Fi/network diagnostics.
- Added live metrics for chip temperature, heap, uptime, generation rate, LittleFS and Wi-Fi uptime.
- Added a system metrics graph to the web interface.
- Added GitHub Actions build and release workflows.
- Added API documentation, tests, issue templates and Docker build support.

## 1.3.0 - 2026-09-22

## 1.2.0

- Added QR code generation on the ESP32 and SVG download.
- Added the More IT Tools toolbox.

## 1.1.0

- Added OTA updates, mDNS, saved Wi-Fi settings and dual AP/STA mode.

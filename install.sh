#!/usr/bin/env bash
set -euo pipefail

# One-command installer for macOS, Debian/Ubuntu and Raspberry Pi OS.
# Usage:
#   ./install.sh                         # detect host and install matching parts
#   ./install.sh --esp32                 # also build and flash ESP32 if a port is found
#   ./install.sh --gateway-only          # only deploy Debian Gateway
#   ./install.sh --docker                # use optional Docker deployment
#   ./install.sh --project-dir /opt/my_it_tools

REPO_URL="${REPO_URL:-https://github.com/iwizard7/my_it_tools.git}"
PROJECT_DIR="${PROJECT_DIR:-$PWD/my_it_tools}"
FLASH_ESP32=0
GATEWAY_ONLY=0
USE_DOCKER=0

log() { printf '\n[%s] %s\n' "$(date '+%H:%M:%S')" "$*"; }
die() { echo "ERROR: $*" >&2; exit 1; }
has() { command -v "$1" >/dev/null 2>&1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --esp32) FLASH_ESP32=1; shift ;;
    --gateway-only) GATEWAY_ONLY=1; shift ;;
    --docker) USE_DOCKER=1; shift ;;
    --project-dir) PROJECT_DIR="${2:?missing project directory}"; shift 2 ;;
    --repo) REPO_URL="${2:?missing repository URL}"; shift 2 ;;
    -h|--help)
      sed -n '1,18p' "$0"
      exit 0
      ;;
    *) die "Unknown argument: $1" ;;
  esac
done

[[ "$(id -u)" -ne 0 ]] || die "Run as a normal user with sudo access, not as root."

OS="$(uname -s)"
ARCH="$(uname -m)"
TARGET="unknown"
if [[ "$OS" == "Darwin" ]]; then
  TARGET="macos"
elif [[ "$OS" == "Linux" ]]; then
  if [[ -f /proc/device-tree/model ]] && grep -qi 'raspberry pi' /proc/device-tree/model; then
    TARGET="raspberry-pi"
  elif [[ -f /sys/firmware/devicetree/base/model ]] && grep -qi 'raspberry pi' /sys/firmware/devicetree/base/model; then
    TARGET="raspberry-pi"
  else
    TARGET="debian-linux"
  fi
fi

log "Detected platform: ${TARGET} (${OS}/${ARCH})"

clone_project() {
  if [[ -f "$PROJECT_DIR/README.md" && -d "$PROJECT_DIR/esp32" ]]; then
    log "Using existing project: $PROJECT_DIR"
    git -C "$PROJECT_DIR" pull --ff-only 2>/dev/null || true
  else
    mkdir -p "$(dirname "$PROJECT_DIR")"
    log "Cloning $REPO_URL into $PROJECT_DIR"
    git clone "$REPO_URL" "$PROJECT_DIR"
  fi
}

install_macos_tools() {
  has brew || die "Homebrew is required on macOS: https://brew.sh"
  brew install python@3.12 git 2>/dev/null || true
  local venv="$HOME/.local/share/my-it-tools/venv"
  python3 -m venv "$venv"
  "$venv/bin/pip" install -q --upgrade pip platformio
  echo "PlatformIO installed at $venv/bin/pio"
  if [[ "$GATEWAY_ONLY" -eq 0 ]]; then
    "$venv/bin/pio" run -d "$PROJECT_DIR/esp32"
  fi
  if [[ "$FLASH_ESP32" -eq 1 ]]; then
    local port
    port="$(ls /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -n1 || true)"
    [[ -n "$port" ]] || die "ESP32 USB port not found. Connect the controller or omit --esp32."
    "$venv/bin/pio" run -d "$PROJECT_DIR/esp32" -t upload --upload-port "$port"
  fi
}

install_linux_tools() {
  sudo apt-get update
  sudo apt-get install -y git curl ca-certificates python3 python3-venv
  if [[ "$USE_DOCKER" -eq 1 ]] && ! has docker; then
    log "Installing Docker with the official Docker installer"
    curl -fsSL https://get.docker.com | sudo sh
  fi
  if [[ "$USE_DOCKER" -eq 1 ]] && ! docker compose version >/dev/null 2>&1; then
    sudo apt-get install -y docker-compose-plugin || true
  fi
  if [[ "$USE_DOCKER" -eq 1 ]]; then
    docker compose version >/dev/null 2>&1 || die "Docker Compose plugin is not available"
    sudo systemctl enable --now docker
  fi
  sudo usermod -aG docker "$USER" || true
  if [[ "$GATEWAY_ONLY" -eq 0 && "$FLASH_ESP32" -eq 1 ]]; then
    local venv="$HOME/.local/share/my-it-tools/venv"
    python3 -m venv "$venv"
    "$venv/bin/pip" install -q --upgrade pip platformio
    "$venv/bin/pio" run -d "$PROJECT_DIR/esp32"
    local port
    port="$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n1 || true)"
    [[ -n "$port" ]] || die "ESP32 USB port not found. Connect the controller or omit --esp32."
    "$venv/bin/pio" run -d "$PROJECT_DIR/esp32" -t upload --upload-port "$port"
  fi
}

start_native_gateway() {
  local venv="$PROJECT_DIR/.gateway-venv"
  log "Installing Debian Gateway in a native Python virtualenv"
  python3 -m venv "$venv"
  "$venv/bin/pip" install -q --upgrade pip
  "$venv/bin/pip" install -q -r "$PROJECT_DIR/debian-gateway/requirements.txt"
  mkdir -p "$PROJECT_DIR/debian-gateway/data"
  if [[ -f "$PROJECT_DIR/.gateway.pid" ]] && kill -0 "$(cat "$PROJECT_DIR/.gateway.pid")" 2>/dev/null; then
    log "Gateway is already running"
  else
    nohup "$venv/bin/uvicorn" app.main:app --app-dir "$PROJECT_DIR/debian-gateway" --host 0.0.0.0 --port 8080 \
      >"$PROJECT_DIR/debian-gateway/gateway.log" 2>&1 &
    echo $! > "$PROJECT_DIR/.gateway.pid"
  fi
  sleep 3
  curl --fail --silent "http://127.0.0.1:8080/healthz" || die "Native Gateway failed health check"
  echo
}

start_gateway() {
  if [[ "$USE_DOCKER" -eq 0 ]]; then
    start_native_gateway
    return
  fi
  cd "$PROJECT_DIR/debian-gateway"
  mkdir -p data
  if [[ "$TARGET" == "raspberry-pi" ]]; then
    log "Starting ARM Raspberry Pi Gateway"
    sudo docker compose -f docker-compose.rpi.yml up -d --build
  else
    log "Starting Debian Gateway"
    sudo docker compose up -d --build
  fi
  sleep 3
  curl --fail --silent "http://127.0.0.1:8080/healthz" || die "Gateway failed health check"
  echo
}

clone_project
case "$TARGET" in
  macos) install_macos_tools ;;
  debian-linux|raspberry-pi) install_linux_tools ;;
  *) die "Unsupported host OS: $OS" ;;
esac

if [[ "$GATEWAY_ONLY" -eq 0 || "$GATEWAY_ONLY" -eq 1 ]]; then
  start_gateway
fi

cat <<EOF

Installation complete.
Project: $PROJECT_DIR
Host:    $TARGET ($ARCH)

ESP32 UI:       connect to ESP32-Random-Tools and open http://192.168.4.1
Debian Gateway: http://$(hostname -I 2>/dev/null | awk '{print $1}' || echo 'HOST_IP'):8080

If Docker group access was just added, log out and in again before using
docker without sudo.
EOF

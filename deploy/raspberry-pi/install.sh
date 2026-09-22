#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${PROJECT_DIR:-$HOME/my_it_tools}"
REPO_URL="${REPO_URL:-https://github.com/iwizard7/my_it_tools.git}"

if [[ "$(id -u)" -eq 0 ]]; then
  echo "Run this script as a normal user with sudo access, not as root."
  exit 1
fi

sudo apt-get update
sudo apt-get install -y git ca-certificates curl docker.io docker-compose-plugin
sudo systemctl enable --now docker
sudo usermod -aG docker "$USER" || true

if [[ ! -d "$PROJECT_DIR/.git" ]]; then
  git clone "$REPO_URL" "$PROJECT_DIR"
else
  git -C "$PROJECT_DIR" pull --ff-only
fi

cd "$PROJECT_DIR/debian-gateway"
mkdir -p data
docker compose -f docker-compose.rpi.yml up -d --build

echo
echo "Debian Gateway is starting on port 8080."
echo "Health check: curl http://127.0.0.1:8080/healthz"
echo "If Docker group access was just added, log out and in again."

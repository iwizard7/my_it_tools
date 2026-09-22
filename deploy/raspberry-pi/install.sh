#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${PROJECT_DIR:-$HOME/my_it_tools}"
REPO_URL="${REPO_URL:-https://github.com/iwizard7/my_it_tools.git}"

if [[ "$(id -u)" -eq 0 ]]; then
  echo "Run this script as a normal user with sudo access, not as root."
  exit 1
fi

if [[ ! -d "$PROJECT_DIR/.git" ]]; then
  mkdir -p "$(dirname "$PROJECT_DIR")"
  git clone "$REPO_URL" "$PROJECT_DIR"
fi

exec "$PROJECT_DIR/install.sh" --project-dir "$PROJECT_DIR"

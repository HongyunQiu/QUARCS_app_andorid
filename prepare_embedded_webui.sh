#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_FRONTEND_DIR="$(cd "$PROJECT_DIR/../QUARCS_stellarium-web-engine/apps/web-frontend" 2>/dev/null && pwd || true)"
FRONTEND_DIR="${QUARCS_FRONTEND_DIR:-$DEFAULT_FRONTEND_DIR}"

if [[ ! -d "$FRONTEND_DIR" ]]; then
  echo "Frontend directory not found: $FRONTEND_DIR" >&2
  exit 1
fi

echo "Syncing embedded web UI from: $FRONTEND_DIR"
(cd "$FRONTEND_DIR" && QUARCS_APP_DIR="$PROJECT_DIR" npm run build:android-embedded)

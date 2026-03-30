#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_FRONTEND_DIR="$(cd "$PROJECT_DIR/../QUARCS_stellarium-web-engine/apps/web-frontend" 2>/dev/null && pwd || true)"
FRONTEND_DIR="${QUARCS_FRONTEND_DIR:-$DEFAULT_FRONTEND_DIR}"
LITE_UI_DIR="$PROJECT_DIR/debug-webui"
UI_MODE="${QUARCS_EMBEDDED_UI_MODE:-full}"
DIST_DIR=""

if [[ "$UI_MODE" == "lite" ]]; then
  [[ -d "$LITE_UI_DIR" ]] || { echo "Lite UI directory not found: $LITE_UI_DIR" >&2; exit 1; }
  echo "Syncing lightweight embedded web UI from: $LITE_UI_DIR"
  DIST_DIR="$LITE_UI_DIR"
else
  if [[ ! -d "$FRONTEND_DIR" ]]; then
    echo "Frontend directory not found: $FRONTEND_DIR" >&2
    exit 1
  fi

  echo "Syncing embedded web UI from: $FRONTEND_DIR"
  (cd "$FRONTEND_DIR" && QUARCS_APP_DIR="$PROJECT_DIR" npm run build:android-embedded)
  DIST_DIR="$FRONTEND_DIR/dist"
fi

ANDROID_SOURCES_ASSET_ROOT="$PROJECT_DIR/android-sources/assets"
ANDROID_BUILD_ASSET_ROOT="$PROJECT_DIR/build-qt6-android-arm64/android-build/assets"
WEBUI_TARGETS=(
  "$ANDROID_SOURCES_ASSET_ROOT/webui"
  "$ANDROID_BUILD_ASSET_ROOT/webui"
)
MANIFEST_TARGETS=(
  "$ANDROID_SOURCES_ASSET_ROOT/webui-manifest.json"
  "$ANDROID_BUILD_ASSET_ROOT/webui-manifest.json"
)

mkdir -p "$ANDROID_SOURCES_ASSET_ROOT"
mkdir -p "$ANDROID_BUILD_ASSET_ROOT"

for target in "${WEBUI_TARGETS[@]}"; do
  python3 - "$target" <<'PY'
import shutil
import sys
from pathlib import Path

target = Path(sys.argv[1])
if target.exists():
    shutil.rmtree(target)
PY
  mkdir -p "$target"
  cp -a "$DIST_DIR"/. "$target"/
done

python3 - "$DIST_DIR" "${MANIFEST_TARGETS[@]}" <<'PY'
import json
import sys
from pathlib import Path

source_dir = Path(sys.argv[1])
manifest_paths = [Path(p) for p in sys.argv[2:]]

entries = []
total_bytes = 0

for path in sorted(source_dir.rglob("*")):
    if not path.is_file():
        continue
    size = path.stat().st_size
    total_bytes += size
    entries.append({
        "path": path.relative_to(source_dir).as_posix(),
        "size": size,
    })

manifest = {
    "mode": "lite" if source_dir.name == "debug-webui" else "full",
    "root": source_dir.as_posix(),
    "fileCount": len(entries),
    "totalBytes": total_bytes,
    "entries": entries,
}

content = json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
for manifest_path in manifest_paths:
    manifest_path.write_text(content, encoding="utf-8")
PY

for target in "${WEBUI_TARGETS[@]}"; do
  echo "Embedded web UI synced to: $target"
done
for manifest in "${MANIFEST_TARGETS[@]}"; do
  echo "Manifest written to: $manifest"
done

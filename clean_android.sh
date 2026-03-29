#!/usr/bin/env bash
set -euo pipefail

# 清理 Android 构建产物
# 用法：
#   ./clean_android.sh

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_ROOT="${PROJECT_DIR}/build-qt6-android-arm64"

log() { echo "[$(date '+%F %T')] $*"; }

if [[ -d "$BUILD_ROOT" ]]; then
  log "删除旧构建目录: $BUILD_ROOT"
  rm -rf "$BUILD_ROOT"
  log "清理完成"
else
  log "无需清理，目录不存在: $BUILD_ROOT"
fi

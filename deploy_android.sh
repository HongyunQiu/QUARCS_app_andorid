#!/usr/bin/env bash
set -euo pipefail

# QUARCS_app 一键部署（构建 + 安装 + 启动）
# 用法：
#   ./deploy_android.sh
#   CLEAN=1 ./deploy_android.sh
#   DEVICE='<serial>' ./deploy_android.sh
#   APK_OUT=/tmp/quarcs.apk ./deploy_android.sh

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

log() { echo "[$(date '+%F %T')] $*"; }

cd "$PROJECT_DIR"

log "STEP 1/3: 构建 APK"
CLEAN="${CLEAN:-0}" APK_OUT="${APK_OUT:-}" ./build_android.sh

APK_PATH="${APK:-$PROJECT_DIR/build-qt6-android-arm64/android-build/build/outputs/apk/debug/android-build-debug.apk}"
[[ -f "$APK_PATH" ]] || { echo "ERROR: APK 不存在: $APK_PATH"; exit 1; }

log "STEP 2/3: 安装到设备"
APK="$APK_PATH" DEVICE="${DEVICE:-}" TARGET="${TARGET:-}" ./install_android.sh

# install_android.sh 的设备选择逻辑（保持一致）
SEL_DEVICE="${DEVICE:-}"
if [[ -z "$SEL_DEVICE" ]]; then
  SEL_DEVICE=$(adb devices | awk -F '\t' 'NR>1 && $2=="device" {print $1; exit}')
fi

if [[ -n "$SEL_DEVICE" ]]; then
  log "STEP 3/3: 启动 App"
  adb -s "$SEL_DEVICE" shell am start -n org.qtproject.example.QUARCS_app/org.qtproject.qt.android.bindings.QtActivity >/dev/null
  log "部署完成：已启动 org.qtproject.example.QUARCS_app"
else
  log "WARN: 未解析到设备序列号，跳过自动启动。"
fi

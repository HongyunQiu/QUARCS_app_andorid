#!/usr/bin/env bash
set -euo pipefail

# QUARCS_app 安卓一键安装脚本
# 用法：
#   ./install_android.sh
#   APK=/path/to/app.apk ./install_android.sh
#   DEVICE=<serial> ./install_android.sh
#   TARGET=192.168.1.88:5555 ./install_android.sh   # 先 adb connect 再安装

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_APK="${PROJECT_DIR}/build-qt6-android-arm64/android-build/build/outputs/apk/debug/android-build-debug.apk"
APK_PATH="${APK:-$DEFAULT_APK}"
DEVICE="${DEVICE:-}"
TARGET="${TARGET:-}"

log() { echo "[$(date '+%F %T')] $*"; }
fail() { log "ERROR: $*"; exit 1; }

# 自动补齐 adb 路径（常见 Ubuntu + Android SDK 路径）
if ! command -v adb >/dev/null 2>&1; then
  for p in \
    "$HOME/Android/Sdk/platform-tools" \
    "$HOME/Library/Android/sdk/platform-tools" \
    "/opt/android-sdk/platform-tools" \
    "/usr/lib/android-sdk/platform-tools"
  do
    if [[ -x "$p/adb" ]]; then
      export PATH="$p:$PATH"
      break
    fi
  done
fi

command -v adb >/dev/null 2>&1 || fail "未找到 adb。请安装 Android platform-tools，或把 adb 加到 PATH。"
[[ -f "$APK_PATH" ]] || fail "APK 不存在: $APK_PATH"

log "ADB 路径: $(command -v adb)"

if [[ -n "$TARGET" ]]; then
  log "连接设备: adb connect $TARGET"
  adb connect "$TARGET" || true
fi

log "当前设备列表:"
adb devices

if [[ -z "$DEVICE" ]]; then
  # 兼容无线配对序列号中包含空格/括号的情况（按 TAB 分列）
  DEVICE=$(adb devices | awk -F '\t' 'NR>1 && $2=="device" {print $1; exit}')
fi

[[ -n "$DEVICE" ]] || fail "未发现可用设备。可用 TARGET=ip:port 或 DEVICE=serial 指定。"

log "使用设备: $DEVICE"
log "安装 APK: $APK_PATH"
set +e
INSTALL_OUTPUT="$(adb -s "$DEVICE" install -r "$APK_PATH" 2>&1)"
INSTALL_STATUS=$?
set -e
printf '%s\n' "$INSTALL_OUTPUT"

if [[ $INSTALL_STATUS -ne 0 ]]; then
  if grep -Fq "INSTALL_FAILED_ABORTED: User rejected permissions" <<<"$INSTALL_OUTPUT"; then
    fail "手机拒绝了 ADB 覆盖安装。请在设备上允许安装，或打开系统里的“USB 安装/ADB 安装”后重试。"
  fi
  fail "ADB 安装失败。"
fi

log "安装完成，检查包名..."
adb -s "$DEVICE" shell pm list packages | grep -i -E 'quarcs|org.qtproject.example.QUARCS_app' || true

log "可选启动命令："
log "adb -s $DEVICE shell am start -n org.qtproject.example.QUARCS_app/org.qtproject.qt.android.bindings.QtActivity"

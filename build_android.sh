#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_ROOT="${PROJECT_DIR}/build-qt6-android-arm64"
ANDROID_BUILD_DIR="${BUILD_ROOT}/android-build"
APK_PATH="${ANDROID_BUILD_DIR}/build/outputs/apk/debug/android-build-debug.apk"
DEPLOY_SETTINGS="${BUILD_ROOT}/android-QUARCS_app-deployment-settings.json"
LOG_DIR="${PROJECT_DIR}/doc"
LOG_FILE="${LOG_DIR}/build_android_$(date +%Y%m%d_%H%M%S).log"
QT_ANDROID_QMAKE="${QT_ANDROID_QMAKE:-/home/q/Qt/6.5.3/android_arm64_v8a/bin/qmake}"
ANDROID_ABIS="${ANDROID_ABIS:-arm64-v8a}"
MAKE_JOBS="${MAKE_JOBS:-2}"

mkdir -p "$LOG_DIR"

log() { echo "[$(date '+%F %T')] $*"; }
fail() { log "ERROR: $*"; log "请查看日志: $LOG_FILE"; exit 1; }

resolve_default_ndk() {
  local sdk_root="$1"
  local candidate

  for candidate in \
    "$sdk_root/ndk/r27c" \
    "$sdk_root/ndk/r27d" \
    "$sdk_root/ndk/android-ndk-r27d"
  do
    [[ -d "$candidate" ]] && { echo "$candidate"; return 0; }
  done

  for candidate in "$sdk_root"/ndk/*; do
    [[ -d "$candidate" ]] && printf '%s\n' "$candidate"
  done | tail -n 1
}

{
  log "===== QUARCS_app Android Build START ====="
  log "PROJECT_DIR=$PROJECT_DIR"
  log "ANDROID_BUILD_DIR=$ANDROID_BUILD_DIR"

  if [[ -d /usr/lib/jvm/java-17-openjdk-amd64 ]]; then
    export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
    export PATH="$JAVA_HOME/bin:$PATH"
    log "使用 JDK17: $JAVA_HOME"
  else
    log "WARN: 未找到 JDK17，继续使用系统默认 java"
  fi

  command -v java >/dev/null 2>&1 || fail "未找到 java"
  java -version || true
  command -v adb >/dev/null 2>&1 || log "WARN: 未找到 adb（不影响构建，但会影响安装验证）"

  if [[ -z "${ANDROID_SDK_ROOT:-}" ]]; then
    if [[ -d "$HOME/Android/Sdk" ]]; then
      export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
    elif [[ -f "${ANDROID_BUILD_DIR}/local.properties" ]]; then
      export ANDROID_SDK_ROOT="$(sed -n 's/^sdk.dir=//p' "${ANDROID_BUILD_DIR}/local.properties" | head -n1)"
    fi
  fi

  if [[ -z "${ANDROID_NDK_ROOT:-}" && -n "${ANDROID_SDK_ROOT:-}" ]]; then
    export ANDROID_NDK_ROOT="$(resolve_default_ndk "$ANDROID_SDK_ROOT")"
  fi

  [[ -n "${ANDROID_SDK_ROOT:-}" ]] || fail "无法解析 ANDROID_SDK_ROOT"
  [[ -d "$ANDROID_SDK_ROOT" ]] || fail "ANDROID_SDK_ROOT 不存在: $ANDROID_SDK_ROOT"
  [[ -n "${ANDROID_NDK_ROOT:-}" ]] || fail "无法解析 ANDROID_NDK_ROOT"
  [[ -d "$ANDROID_NDK_ROOT" ]] || fail "ANDROID_NDK_ROOT 不存在: $ANDROID_NDK_ROOT"
  [[ -x "$QT_ANDROID_QMAKE" ]] || fail "未找到可执行的 qmake: $QT_ANDROID_QMAKE"

  log "同步内置前端资源"
  bash "${PROJECT_DIR}/prepare_embedded_webui.sh"

  if [[ "${CLEAN:-0}" == "1" ]]; then
    log "执行清理: 删除旧构建目录 $BUILD_ROOT"
    rm -rf "$BUILD_ROOT"
  fi

  mkdir -p "$BUILD_ROOT"

  if [[ ! -f "$BUILD_ROOT/Makefile" || ! -f "$DEPLOY_SETTINGS" ]]; then
    log "生成 Android 构建文件: qmake ANDROID_ABIS=$ANDROID_ABIS"
    (cd "$BUILD_ROOT" && "$QT_ANDROID_QMAKE" "../QUARCS_app.pro" "ANDROID_ABIS=${ANDROID_ABIS}")
  fi

  [[ -f "$DEPLOY_SETTINGS" ]] || fail "未找到部署设置文件: $DEPLOY_SETTINGS"

  log "执行构建: make apk -j$MAKE_JOBS"
  (cd "$BUILD_ROOT" && make apk -j"$MAKE_JOBS")

  [[ -f "$APK_PATH" ]] || fail "构建完成但未找到 APK: $APK_PATH"

  log "APK 构建成功: $APK_PATH"

  if [[ -n "${APK_OUT:-}" ]]; then
    cp -f "$APK_PATH" "$APK_OUT"
    log "已复制 APK 到: $APK_OUT"
  fi

  log "可选安装命令: adb install -r $APK_PATH"
  log "===== QUARCS_app Android Build DONE ====="
} 2>&1 | tee "$LOG_FILE"

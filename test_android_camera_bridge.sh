#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEVICE="${DEVICE:-}"
DEPLOY_FIRST="${DEPLOY_FIRST:-1}"
HTTP_PORT="${HTTP_PORT:-18080}"
WS_PORT="${WS_PORT:-8600}"
EXPOSURE_SECONDS="${EXPOSURE_SECONDS:-1.5}"
CAMERA_BACKEND="${CAMERA_BACKEND:-qhyccd}"
OPEN_APP_PAGE="${OPEN_APP_PAGE:-1}"

log() { echo "[$(date '+%F %T')] $*"; }
fail() { log "ERROR: $*"; exit 1; }

command -v adb >/dev/null 2>&1 || fail "未找到 adb"
command -v curl >/dev/null 2>&1 || fail "未找到 curl"
command -v node >/dev/null 2>&1 || fail "未找到 node"

if [[ -z "$DEVICE" ]]; then
  DEVICE="$(adb devices | awk -F '\t' 'NR>1 && $2=="device" {print $1; exit}')"
fi
[[ -n "$DEVICE" ]] || fail "未发现可用 Android 设备"

if [[ "$DEPLOY_FIRST" == "1" ]]; then
  log "STEP 1/5: 构建、安装并启动轻量 APK"
  if ! QUARCS_EMBEDDED_UI_MODE=lite DEVICE="$DEVICE" QUARCS_CAMERA_BACKEND="$CAMERA_BACKEND" "$PROJECT_DIR/deploy_android.sh"; then
    fail "部署失败"
  fi
else
  log "STEP 1/5: 跳过部署，直接启动 App"
  adb -s "$DEVICE" shell am start -n org.qtproject.example.QUARCS_app/org.qtproject.qt.android.bindings.QtActivity >/dev/null
fi

log "STEP 2/5: 转发设备端口"
adb -s "$DEVICE" forward "tcp:${HTTP_PORT}" "tcp:${HTTP_PORT}" >/dev/null
adb -s "$DEVICE" forward "tcp:${WS_PORT}" "tcp:${WS_PORT}" >/dev/null

URL="http://127.0.0.1:${HTTP_PORT}/index.html"
log "等待页面就绪: $URL"
for _ in $(seq 1 30); do
  if curl --max-time 3 -fsS "$URL" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
curl --max-time 3 -fsS "$URL" >/dev/null 2>&1 || fail "页面未就绪或返回为空: $URL"

if [[ "$OPEN_APP_PAGE" == "1" ]]; then
  log "STEP 3/5: 保持 App 页面已打开"
else
  log "STEP 3/5: 跳过页面可见性要求，仅验证桥接服务"
fi

log "STEP 4/5: 执行相机桥接自动化"
export QUARCS_AUTOMATION_WS_URL="ws://127.0.0.1:${WS_PORT}"
export QUARCS_AUTOMATION_EXPOSURE_SECONDS="$EXPOSURE_SECONDS"

AUTOMATION_OUTPUT="$(node <<'JS'
const wsUrl = process.env.QUARCS_AUTOMATION_WS_URL;
const exposureSeconds = String(process.env.QUARCS_AUTOMATION_EXPOSURE_SECONDS || '1.5');
const exposureMillis = Math.max(500, Math.round(Number(exposureSeconds) * 1000) || 1500);

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function waitFor(predicate, timeoutMs, stepMs = 200) {
  const startedAt = Date.now();
  while ((Date.now() - startedAt) < timeoutMs) {
    if (predicate()) {
      return true;
    }
    await sleep(stepMs);
  }
  return predicate();
}

async function main() {
  const logs = [];
  const returns = [];
  const ws = new WebSocket(wsUrl);

  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`WebSocket connect timeout: ${wsUrl}`)), 10000);
    ws.onopen = () => {
      clearTimeout(timer);
      logs.push(`[bridge] connected ${wsUrl}`);
      resolve();
    };
    ws.onerror = event => {
      clearTimeout(timer);
      reject(new Error(`WebSocket error: ${event.type}`));
    };
  });

  ws.onmessage = event => {
    const text = String(event.data);
    try {
      const payload = JSON.parse(text);
      if (payload.type === 'QT_Log') {
        logs.push(payload.message);
      } else if (payload.type === 'QT_Return') {
        returns.push(payload.message);
        logs.push(`[return] ${payload.message}`);
      } else {
        logs.push(`[message] ${text}`);
      }
    } catch (error) {
      logs.push(`[message] ${text}`);
    }
  };

  function sendVueCommand(message) {
    logs.push(`[send] ${message}`);
    ws.send(JSON.stringify({ type: 'Vue_Command', message }));
  }

  sendVueCommand('USBCheck');
  await sleep(500);
  sendVueCommand('getMainCameraParameters');
  await sleep(600);
  sendVueCommand('getCaptureStatus');
  await sleep(400);
  sendVueCommand(`takeExposure:${exposureSeconds}`);
  await sleep(Math.max(1000, Math.floor(exposureMillis * 0.8)));
  sendVueCommand('getCaptureStatus');
  await sleep(600);
  sendVueCommand('getOriginalImage');
  const imageReturned = await waitFor(
    () => returns.some(line => line.startsWith('OriginalImage:')) ||
          returns.some(line => line.startsWith('OriginalImageInfo:')),
    Math.max(12000, exposureMillis + 10000)
  );
  if (!imageReturned) {
    sendVueCommand('abortExposure');
    await sleep(800);
  }

  ws.close();
  await sleep(200);

  const output = {
    logs,
    returns,
    summary: {
      usbCheckSeen: returns.some(line => line.startsWith('USBCheck:')),
      mainCameraParametersSeen: returns.some(line => line.startsWith('MainCameraParameters:')),
      captureStatusSeen: returns.some(line => line.startsWith('CaptureStatus:')),
      originalImageSeen: returns.some(line => line.startsWith('OriginalImage:')),
      originalImageInfoSeen: returns.some(line => line.startsWith('OriginalImageInfo:')),
      qhyBackendSeen: returns.some(line => line.startsWith('USBCheck:QHY')) ||
                     returns.some(line => line.startsWith('MainCameraParameters:QHY')),
      exposureStarted: returns.some(line => line.includes('CaptureStatus:true:Exposing')),
      frameDownloaded: returns.some(line => line.startsWith('OriginalImageInfo:')) ||
                      returns.some(line => line.startsWith('OriginalImage:'))
    }
  };

  process.stdout.write(JSON.stringify(output, null, 2));
}

main().catch(error => {
  console.error(error.stack || String(error));
  process.exit(1);
});
JS
)"

LOG_PATH="$PROJECT_DIR/doc/android_camera_bridge_test_$(date +%Y%m%d_%H%M%S).json"
printf '%s\n' "$AUTOMATION_OUTPUT" > "$LOG_PATH"

log "STEP 5/5: 自动化结果已保存: $LOG_PATH"
echo
echo "===== Automation Summary ====="
node -e 'const fs=require("fs"); const data=JSON.parse(fs.readFileSync(process.argv[1],"utf8")); console.log(JSON.stringify(data.summary,null,2)); console.log("\n===== Recent Logs ====="); console.log(data.logs.slice(-60).join("\n"));' "$LOG_PATH"

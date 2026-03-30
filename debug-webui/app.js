(function () {
  const config = window.__QUARCS_RUNTIME_CONFIG__ || {};
  const wsProtocol = config.wsProtocol || "ws:";
  const wsHost = config.wsHost || "127.0.0.1";
  const wsPort = config.wsPort || 8600;
  const wsUrl = `${wsProtocol}//${wsHost}:${wsPort}`;

  const wsStatus = document.getElementById("ws-status");
  const baseUrl = document.getElementById("base-url");
  const logOutput = document.getElementById("log-output");
  const serialPort = document.getElementById("serial-port");
  const baudRate = document.getElementById("baud-rate");
  const tcpEndpoint = document.getElementById("tcp-endpoint");
  const moveSteps = document.getElementById("move-steps");
  const mountSerialPort = document.getElementById("mount-serial-port");
  const mountBaudRate = document.getElementById("mount-baud-rate");
  const mountTcpEndpoint = document.getElementById("mount-tcp-endpoint");
  const mountGotoRa = document.getElementById("mount-goto-ra");
  const mountGotoDec = document.getElementById("mount-goto-dec");
  const cameraExposureMs = document.getElementById("camera-exposure-ms");
  const cameraGain = document.getElementById("camera-gain");
  const cameraOffset = document.getElementById("camera-offset");
  const cameraBinning = document.getElementById("camera-binning");
  const cameraFrameType = document.getElementById("camera-frame-type");
  const cameraCooler = document.getElementById("camera-cooler");
  const cameraTargetTemp = document.getElementById("camera-target-temp");
  const cameraRoiX = document.getElementById("camera-roi-x");
  const cameraRoiY = document.getElementById("camera-roi-y");
  const cameraRoiW = document.getElementById("camera-roi-w");
  const cameraRoiH = document.getElementById("camera-roi-h");
  const cameraMeta = document.getElementById("camera-meta");
  const cameraImageMeta = document.getElementById("camera-image-meta");
  const cameraPreview = document.getElementById("camera-preview");
  const cameraPreviewStretched = document.getElementById("camera-preview-stretched");

  let socket = null;
  const cameraState = {
    name: "Unknown",
    connected: false,
    gain: 0,
    offset: 0,
    width: 0,
    height: 0,
    binX: 1,
    binY: 1,
    bitDepth: 16,
    exposureSeconds: 0,
    captureState: "Idle",
    frameReady: false,
    frameType: "Light",
    targetTemperature: 0,
    sensorTemperature: 0,
    hasShutter: null,
    cooler: null,
    capabilities: null,
    rawBytes: 0,
    rawHash: "",
    rawPath: ""
  };

  baseUrl.textContent = `WS ${wsUrl}`;

  function stamp() {
    return new Date().toLocaleTimeString();
  }

  function appendLog(message, tone) {
    const line = `[${stamp()}] ${message}`;
    logOutput.textContent += `${line}\n`;
    logOutput.scrollTop = logOutput.scrollHeight;
    if (tone === "error") {
      console.error(message);
    } else {
      console.log(message);
    }
  }

  function setStatus(label, mode) {
    wsStatus.textContent = label;
    wsStatus.className = `status-pill ${mode}`;
  }

  function summarizeReturnMessage(message) {
    if (message.startsWith("OriginalImage:")) {
      const payload = message.slice("OriginalImage:".length);
      return `OriginalImage:<base64 ${payload.length} chars>`;
    }
    if (message.startsWith("OriginalImageStretched:")) {
      const payload = message.slice("OriginalImageStretched:".length);
      return `OriginalImageStretched:<base64 ${payload.length} chars>`;
    }
    return message;
  }

  function renderCameraState() {
    const capabilitySummary = cameraState.capabilities
      ? (() => {
          const gainRange = cameraState.capabilities.ranges && cameraState.capabilities.ranges.Gain;
          const offsetRange = cameraState.capabilities.ranges && cameraState.capabilities.ranges.Offset;
          const exposureRange = cameraState.capabilities.ranges && cameraState.capabilities.ranges.ExposureSeconds;
          return [
            `Caps: exp=${cameraState.capabilities.exposure} gain=${cameraState.capabilities.gain} offset=${cameraState.capabilities.offset} cooler=${cameraState.capabilities.cooler} roi=${cameraState.capabilities.roi} bin<=${cameraState.capabilities.maxBinX}x${cameraState.capabilities.maxBinY}`,
            exposureRange ? `Exposure Range: ${exposureRange.minimum}..${exposureRange.maximum}s step=${exposureRange.step}` : null,
            gainRange ? `Gain Range: ${gainRange.minimum}..${gainRange.maximum} step=${gainRange.step}` : null,
            offsetRange ? `Offset Range: ${offsetRange.minimum}..${offsetRange.maximum} step=${offsetRange.step}` : null
          ].filter(Boolean).join("\n");
        })()
      : "Caps: unknown";

    cameraMeta.textContent = [
      `Camera: ${cameraState.name}`,
      `Connected: ${cameraState.connected}`,
      `Gain: ${cameraState.gain}  Offset: ${cameraState.offset}`,
      `Frame: ${cameraState.width} x ${cameraState.height}  Bin: ${cameraState.binX}x${cameraState.binY}`,
      `Bit Depth: ${cameraState.bitDepth}`,
      `Frame Type: ${cameraState.frameType}`,
      `Capture: ${cameraState.captureState}  Exposure: ${cameraState.exposureSeconds.toFixed(3)}s`,
      cameraState.cooler ? `Cooler: supported=${cameraState.cooler.supported} enabled=${cameraState.cooler.enabled} sensor=${cameraState.sensorTemperature}C target=${cameraState.targetTemperature}C` : "Cooler: unknown",
      `Shutter: ${cameraState.hasShutter === null ? "unknown" : cameraState.hasShutter}`,
      capabilitySummary
    ].join("\n");

    cameraImageMeta.textContent = cameraState.rawBytes
      ? [
          `Raw Bytes: ${cameraState.rawBytes}`,
          `Raw Hash: ${cameraState.rawHash}`,
          `Raw Cache: ${cameraState.rawPath || "n/a"}`
        ].join("\n")
      : "No frame yet.";
  }

  function requestCameraSnapshot() {
    sendVueCommand("USBCheck");
    sendVueCommand("getMainCameraParameters");
    sendVueCommand("getCameraControlState");
    sendVueCommand("getCameraCapabilities");
    sendVueCommand("getROIInfo");
    sendVueCommand("getCaptureStatus");
  }

  function applyCameraSettings() {
    sendVueCommand(`setCameraGain:${cameraGain.value.trim() || "20"}`);
    sendVueCommand(`setCameraOffset:${cameraOffset.value.trim() || "140"}`);
    sendVueCommand(`setCameraBinning:${cameraBinning.value.trim() || "1"}`);
    sendVueCommand(`setCameraFrameType:${cameraFrameType.value.trim() || "light"}`);
    sendVueCommand(`setCameraCooler:${cameraCooler.value.trim() || "off"}`);
    sendVueCommand(`setCameraTargetTemperature:${cameraTargetTemp.value.trim() || "0"}`);
  }

  function applyCameraRoi() {
    sendVueCommand(`setCameraROI:${cameraRoiX.value.trim() || "0"}:${cameraRoiY.value.trim() || "0"}:${cameraRoiW.value.trim() || "1"}:${cameraRoiH.value.trim() || "1"}`);
  }

  function captureSingleFrame() {
    applyCameraSettings();
    const exposureMs = Math.max(1, Number(cameraExposureMs.value.trim() || "30"));
    const exposureSeconds = exposureMs / 1000;
    window.setTimeout(function () {
      sendVueCommand(`takeExposure:${exposureSeconds}`);
      window.setTimeout(function () {
        sendVueCommand("getOriginalImage");
      }, Math.max(400, exposureMs + 600));
    }, 150);
  }

  function sendVueCommand(command) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      appendLog(`Cannot send, socket not ready: ${command}`, "error");
      return;
    }

    socket.send(JSON.stringify({
      type: "Vue_Command",
      message: command
    }));
    appendLog(`>>> ${command}`);
  }

  function connectSocket() {
    setStatus("Connecting...", "status-idle");
    socket = new WebSocket(wsUrl);

    socket.addEventListener("open", function () {
      setStatus("Bridge Online", "status-live");
      appendLog(`Connected to ${wsUrl}`);
      sendVueCommand("loadSDKVersionAndUSBSerialPath");
      sendVueCommand("loadMountConfig");
      window.setTimeout(requestCameraSnapshot, 180);
    });

    socket.addEventListener("message", function (event) {
      try {
        const payload = JSON.parse(event.data);
        if (payload.type === "QT_Log" && typeof payload.message === "string") {
          appendLog(payload.message);
          return;
        }

        if (payload.type === "QT_Return" && typeof payload.message === "string") {
          appendLog(`<<< ${summarizeReturnMessage(payload.message)}`);
          const parts = payload.message.split(":");
          if (parts[0] === "SDKVersionAndUSBSerialPath") {
            for (let i = 1; i + 2 < parts.length; i += 3) {
              if (parts[i] === "Focuser") {
                if (parts[i + 2] && parts[i + 2].toLowerCase() !== "null") {
                  serialPort.value = parts[i + 2];
                }
              }
            }
          }
          if (parts[0] === "MountTransportConfig") {
            if (parts[1] && parts[1].toLowerCase() !== "null") {
              mountSerialPort.value = parts[1];
            }
            if (parts[2]) {
              mountBaudRate.value = parts[2];
            }
            if (parts[3] && parts[3].toLowerCase() !== "null") {
              mountTcpEndpoint.value = parts[3];
            }
          }
          if (parts[0] === "USBCheck") {
            const usbPayload = payload.message.slice("USBCheck:".length).split(",");
            cameraState.name = usbPayload[0] || cameraState.name;
            cameraState.connected = usbPayload[1] === "1";
            renderCameraState();
          }
          if (parts[0] === "MainCameraParameters") {
            cameraState.name = parts[1] || cameraState.name;
            cameraState.connected = parts[2] === "true";
            cameraState.gain = Number(parts[3] || cameraState.gain);
            cameraState.offset = Number(parts[4] || cameraState.offset);
            cameraState.width = Number(parts[7] || cameraState.width);
            cameraState.height = Number(parts[8] || cameraState.height);
            cameraState.binX = Number(parts[9] || cameraState.binX);
            cameraState.binY = Number(parts[10] || cameraState.binY);
            cameraState.bitDepth = Number(parts[11] || cameraState.bitDepth);
            cameraGain.value = String(cameraState.gain);
            cameraOffset.value = String(cameraState.offset);
            cameraBinning.value = String(cameraState.binX || 1);
            cameraRoiW.value = String(cameraState.width || cameraRoiW.value);
            cameraRoiH.value = String(cameraState.height || cameraRoiH.value);
            renderCameraState();
          }
          if (parts[0] === "ROIInfo") {
            cameraRoiX.value = String(parts[1] || cameraRoiX.value);
            cameraRoiY.value = String(parts[2] || cameraRoiY.value);
            cameraRoiW.value = String(parts[3] || cameraRoiW.value);
            cameraRoiH.value = String(parts[4] || cameraRoiH.value);
            cameraState.width = Number(parts[3] || cameraState.width);
            cameraState.height = Number(parts[4] || cameraState.height);
            cameraState.binX = Number(parts[5] || cameraState.binX);
            cameraState.binY = Number(parts[6] || cameraState.binY);
            renderCameraState();
          }
          if (parts[0] === "CameraFrameType") {
            cameraState.frameType = parts[1] || cameraState.frameType;
            cameraFrameType.value = cameraState.frameType.toLowerCase();
            renderCameraState();
          }
          if (parts[0] === "CameraControlState") {
            cameraState.gain = Number(parts[1] || cameraState.gain);
            cameraState.offset = Number(parts[2] || cameraState.offset);
            cameraRoiX.value = String(parts[3] || cameraRoiX.value);
            cameraRoiY.value = String(parts[4] || cameraRoiY.value);
            cameraRoiW.value = String(parts[5] || cameraRoiW.value);
            cameraRoiH.value = String(parts[6] || cameraRoiH.value);
            cameraState.width = Number(parts[5] || cameraState.width);
            cameraState.height = Number(parts[6] || cameraState.height);
            cameraState.binX = Number(parts[7] || cameraState.binX);
            cameraState.binY = Number(parts[8] || cameraState.binY);
            cameraState.frameType = parts[9] || cameraState.frameType;
            cameraState.targetTemperature = Number(parts[12] || cameraState.targetTemperature);
            cameraGain.value = String(cameraState.gain);
            cameraOffset.value = String(cameraState.offset);
            cameraBinning.value = String(cameraState.binX || 1);
            cameraFrameType.value = cameraState.frameType.toLowerCase();
            cameraTargetTemp.value = String(cameraState.targetTemperature);
            if (!cameraState.cooler) {
              cameraState.cooler = {
                supported: parts[10] === "true",
                enabled: parts[11] === "true",
                temperature: "n/a"
              };
            } else {
              cameraState.cooler.supported = parts[10] === "true";
              cameraState.cooler.enabled = parts[11] === "true";
            }
            cameraCooler.value = cameraState.cooler.enabled ? "on" : "off";
            renderCameraState();
          }
          if (parts[0] === "CameraCapabilityFlags") {
            cameraState.capabilities = {
              exposure: parts[1] === "true",
              gain: parts[2] === "true",
              offset: parts[3] === "true",
              cooler: parts[4] === "true",
              targetTemperature: parts[5] === "true",
              roi: parts[6] === "true",
              binning: parts[7] === "true",
              frameType: parts[8] === "true",
              readModes: parts[9] === "true",
              maxBinX: Number(parts[10] || 1),
              maxBinY: Number(parts[11] || 1),
              ranges: cameraState.capabilities ? cameraState.capabilities.ranges : {},
              frameTypes: cameraState.capabilities ? cameraState.capabilities.frameTypes : [],
              readModeNames: cameraState.capabilities ? cameraState.capabilities.readModeNames : []
            };
            renderCameraState();
          }
          if (parts[0] === "CameraControlRange") {
            if (!cameraState.capabilities) {
              cameraState.capabilities = { ranges: {}, frameTypes: [], readModeNames: [] };
            }
            if (!cameraState.capabilities.ranges) {
              cameraState.capabilities.ranges = {};
            }
            cameraState.capabilities.ranges[parts[1] || "Unknown"] = {
              supported: parts[2] === "true",
              minimum: Number(parts[3] || 0),
              maximum: Number(parts[4] || 0),
              step: Number(parts[5] || 0),
              current: Number(parts[6] || 0)
            };
            renderCameraState();
          }
          if (parts[0] === "CameraFrameTypes") {
            if (!cameraState.capabilities) {
              cameraState.capabilities = { ranges: {}, frameTypes: [], readModeNames: [] };
            }
            cameraState.capabilities.frameTypes = payload.message.slice("CameraFrameTypes:".length).split("|").filter(Boolean);
            renderCameraState();
          }
          if (parts[0] === "CameraReadModes") {
            if (!cameraState.capabilities) {
              cameraState.capabilities = { ranges: {}, frameTypes: [], readModeNames: [] };
            }
            cameraState.capabilities.readModeNames = payload.message.slice("CameraReadModes:".length).split("|").filter(Boolean);
            renderCameraState();
          }
          if (parts[0] === "CameraSensorState") {
            cameraState.sensorTemperature = Number(parts[1] || cameraState.sensorTemperature);
            cameraState.bitDepth = Number(parts[2] || cameraState.bitDepth);
            cameraState.hasShutter = parts[3] === "true";
            if (!cameraState.cooler) {
              cameraState.cooler = {
                supported: false,
                enabled: false,
                temperature: parts[1] || "n/a"
              };
            } else {
              cameraState.cooler.temperature = parts[1] || "n/a";
            }
            renderCameraState();
          }
          if (parts[0] === "CameraCooler") {
            cameraState.cooler = {
              supported: parts[1] === "true",
              enabled: parts[2] === "true",
              temperature: parts[3] || "n/a"
            };
            cameraState.sensorTemperature = Number(parts[3] || cameraState.sensorTemperature);
            cameraCooler.value = cameraState.cooler.enabled ? "on" : "off";
            renderCameraState();
          }
          if (parts[0] === "CaptureStatus") {
            cameraState.captureState = parts[2] || cameraState.captureState;
            cameraState.exposureSeconds = Number(parts[3] || cameraState.exposureSeconds);
            cameraState.frameReady = parts[4] === "true";
            renderCameraState();
          }
          if (parts[0] === "OriginalImage") {
            const pngBase64 = payload.message.slice("OriginalImage:".length);
            cameraPreview.src = `data:image/png;base64,${pngBase64}`;
            cameraPreview.alt = `${cameraState.name} preview`;
          }
          if (parts[0] === "OriginalImageStretched") {
            const pngBase64 = payload.message.slice("OriginalImageStretched:".length);
            cameraPreviewStretched.src = `data:image/png;base64,${pngBase64}`;
            cameraPreviewStretched.alt = `${cameraState.name} preview stretched`;
          }
          if (parts[0] === "OriginalImageInfo") {
            cameraState.rawBytes = Number(parts[1] || 0);
            cameraState.rawHash = parts[2] || "";
            cameraState.rawPath = parts.slice(3).join(":");
            renderCameraState();
          }
        } else {
          appendLog(`<<< ${event.data}`);
        }
      } catch (error) {
        appendLog(`<<< ${String(event.data).slice(0, 400)}`);
        appendLog(`Message parse warning: ${error.message}`, "error");
      }
    });

    socket.addEventListener("close", function () {
      setStatus("Bridge Offline", "status-dead");
      appendLog("Socket closed", "error");
      window.setTimeout(connectSocket, 1200);
    });

    socket.addEventListener("error", function () {
      setStatus("Bridge Error", "status-dead");
      appendLog("Socket error", "error");
    });
  }

  document.querySelectorAll("[data-vue-command]").forEach(function (button) {
    button.addEventListener("click", function () {
      sendVueCommand(button.getAttribute("data-vue-command"));
    });
  });

  document.querySelectorAll("[data-command]").forEach(function (button) {
    button.addEventListener("click", function () {
      const action = button.getAttribute("data-command");

      if (action === "save-serial") {
        const port = serialPort.value.trim();
        if (!port) {
          appendLog("Serial port is empty. Reload config or enter /dev/ttyACM0 first.", "error");
          return;
        }
        sendVueCommand(`saveToConfigFile:FocuserSerialPort:${port}`);
        sendVueCommand(`SetSerialPort:Focuser:${port}`);
        return;
      }

      if (action === "save-baud") {
        sendVueCommand(`saveToConfigFile:FocuserBaudRate:${baudRate.value.trim()}`);
        return;
      }

      if (action === "save-tcp") {
        sendVueCommand(`saveToConfigFile:FocuserTcpEndpoint:${tcpEndpoint.value.trim()}`);
        return;
      }

      if (action === "reload-config") {
        sendVueCommand("loadSDKVersionAndUSBSerialPath");
        return;
      }

      if (action === "save-mount-serial") {
        const port = mountSerialPort.value.trim();
        if (!port) {
          appendLog("Mount serial port is empty. Enter it first.", "error");
          return;
        }
        sendVueCommand(`saveToConfigFile:MountSerialPort:${port}`);
        sendVueCommand(`SetSerialPort:Mount:${port}`);
        return;
      }

      if (action === "save-mount-baud") {
        sendVueCommand(`saveToConfigFile:MountBaudRate:${mountBaudRate.value.trim()}`);
        return;
      }

      if (action === "save-mount-tcp") {
        sendVueCommand(`saveToConfigFile:MountTcpEndpoint:${mountTcpEndpoint.value.trim()}`);
        return;
      }

      if (action === "reload-mount-config") {
        sendVueCommand("loadMountConfig");
        return;
      }

      if (action === "move-left") {
        sendVueCommand(`focusMoveStep:Left:${moveSteps.value.trim() || "100"}`);
        return;
      }

      if (action === "move-right") {
        sendVueCommand(`focusMoveStep:Right:${moveSteps.value.trim() || "100"}`);
        return;
      }

      if (action === "mount-goto") {
        const ra = mountGotoRa.value.trim() || "0";
        const dec = mountGotoDec.value.trim() || "0";
        sendVueCommand(`MountGoto:${ra}:${dec}`);
        return;
      }

      if (action === "clear-log") {
        logOutput.textContent = "";
        return;
      }

      if (action === "camera-refresh") {
        requestCameraSnapshot();
        return;
      }

      if (action === "camera-apply") {
        applyCameraSettings();
        window.setTimeout(requestCameraSnapshot, 200);
        return;
      }

      if (action === "camera-apply-roi") {
        applyCameraRoi();
        window.setTimeout(requestCameraSnapshot, 200);
        return;
      }

      if (action === "camera-capture") {
        captureSingleFrame();
        return;
      }

      if (action === "ping-version") {
        sendVueCommand("getQTClientVersion");
      }
    });
  });
  renderCameraState();
  connectSocket();
})();

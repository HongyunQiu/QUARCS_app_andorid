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

  let socket = null;

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
    });

    socket.addEventListener("message", function (event) {
      try {
        const payload = JSON.parse(event.data);
        if (payload.type === "QT_Log" && typeof payload.message === "string") {
          appendLog(payload.message);
          return;
        }

        appendLog(`<<< ${event.data}`);
        if (payload.type === "QT_Return" && typeof payload.message === "string") {
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
        }
      } catch (error) {
        appendLog(`<<< ${event.data}`);
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

      if (action === "ping-version") {
        sendVueCommand("getQTClientVersion");
      }
    });
  });
  connectSocket();
})();

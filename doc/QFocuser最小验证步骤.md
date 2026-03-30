# QFocuser 最小验证步骤

适用工程：`/home/q/workspace/QUARCS_app_andorid`

目标：验证当前 Android 统一硬件架构下，`QFocuserAdapter -> QFocuserCore -> Transport` 链路可正常工作。

## 1. 当前已支持的验证能力

当前 focuser 已具备以下最小闭环：

- 配置串口或 TCP 端点
- 建立连接并握手
- 读取当前位置
- 相对移动
- 绝对移动
- 停止移动
- 返回基础自检结果

## 2. 配置方式

当前 focuser transport 支持两种来源：

1. `QSettings`
2. 环境变量

优先级：`QSettings` 高于环境变量。

### 2.1 QSettings 键

应用内通过兼容命令写入：

- `saveToConfigFile:FocuserSerialPort:/dev/ttyACM0`
- `saveToConfigFile:FocuserBaudRate:9600`
- `saveToConfigFile:FocuserTcpEndpoint:192.168.1.10:6000`

说明：

- 若使用串口，优先设置 `FocuserSerialPort`
- 若使用 TCP，设置 `FocuserTcpEndpoint`
- 二者同时存在时，当前实现优先使用 TCP

### 2.2 环境变量

- `QUARCS_FOCUSER_SERIAL_PORT`
- `QUARCS_FOCUSER_BAUD`
- `QUARCS_FOCUSER_TCP`

## 3. 推荐验证命令顺序

通过兼容 WebSocket 发送 `Vue_Command` 消息，推荐顺序如下。

### 3.1 保存串口与波特率

```text
saveToConfigFile:FocuserSerialPort:/dev/ttyACM0
saveToConfigFile:FocuserBaudRate:9600
```

若串口由前端下拉框选择，也会自动走：

```text
SetSerialPort:Focuser:/dev/ttyACM0
```

### 3.2 主动连接

```text
FocuserConnect
```

成功返回示例：

```text
FocuserSelfTest:PASS:connected
FocusPosition:12345:12345
```

失败返回示例：

```text
FocuserSelfTest:FAIL:Failed to open focuser transport
```

### 3.3 运行最小自检

```text
FocuserSelfTest
```

成功返回示例：

```text
FocuserSelfTest:PASS:position=12345:target=12345:speed=1:temp=20.0
FocuserParameters:0:10000:0:10:50
FocusPosition:12345:12345
```

### 3.4 做一次步进移动

```text
focusMoveStep:Right:100
```

或：

```text
focusMoveStep:Left:100
```

预期返回：

```text
FocusPosition:12445:12445
```

### 3.5 停止

```text
focusMoveStop:false
```

预期返回：

```text
FocusPosition:12445:12445
```

## 4. 当前验证判定标准

满足以下几点即可认为当前第一阶段链路已打通：

- `FocuserConnect` 返回 `PASS`
- `FocuserSelfTest` 能拿到位置
- `focusMoveStep` 后位置发生变化
- `focusMoveStop` 不报错

## 5. 当前已知限制

- 还没有 Android 原生 USB 权限申请与设备枚举整合到 `UsbSerialTransport`
- 串口列表自动发现尚未接入
- 还没有为 `OnStepMountCore` 提供同等级真实协议实现
- 当前 focuser 自检更偏“链路连通性验证”，还不是完整厂商级验收

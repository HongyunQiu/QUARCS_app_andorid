#include "devicegatewayservice.h"

#include "hardware/devices/device_registry.h"
#include "hardware/devices/focuser_device_interface.h"
#include "hardware/devices/mount_device_interface.h"

#include <QSettings>

bool DeviceGatewayService::canHandle(const QString &command) const
{
    return command == QStringLiteral("connectAllDevice") ||
           command == QStringLiteral("autoConnectAllDevice") ||
           command == QStringLiteral("disconnectAllDevice") ||
           command == QStringLiteral("getConnectedDevices") ||
           command == QStringLiteral("getLastSelectDevice") ||
           command == QStringLiteral("loadSelectedDriverList") ||
           command == QStringLiteral("loadBindDeviceList") ||
           command == QStringLiteral("loadBindDeviceTypeList") ||
           command == QStringLiteral("SelectIndiDriver") ||
           command == QStringLiteral("ConfirmIndiDriver") ||
           command == QStringLiteral("ConfirmIndiDevice") ||
           command == QStringLiteral("BindingDevice") ||
           command == QStringLiteral("UnBindingDevice") ||
           command == QStringLiteral("SetSerialPort") ||
           command == QStringLiteral("disconnectSelectDriver");
}

QStringList DeviceGatewayService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(payload)

    if (command == QStringLiteral("SetSerialPort")) {
        return saveSerialPortSelection(payload);
    }

    if (command == QStringLiteral("connectAllDevice") ||
        command == QStringLiteral("autoConnectAllDevice")) {
        DeviceRegistry::reloadMountDevice();
        DeviceRegistry::reloadFocuserDevice();
        const bool mountOk = DeviceRegistry::mountDevice().connect();
        const bool focuserOk = DeviceRegistry::focuserDevice().connect();

        if (!mountOk || !focuserOk) {
            QString reason;
            if (!mountOk) {
                reason += QStringLiteral("Mount connect failed. ");
            }
            if (!focuserOk) {
                reason += DeviceRegistry::focuserDevice().state().lastError;
            }

            QStringList responses = selectedDriverList();
            responses.append(connectedDevices());
            responses.append(QStringLiteral("ConnectFailed:%1").arg(reason.trimmed()));
            return responses;
        }

        QStringList responses = selectedDriverList();
        responses.append(connectedDevices());
        responses.append(QStringLiteral("ConnectAllDeviceComplete"));
        return responses;
    }

    if (command == QStringLiteral("disconnectAllDevice")) {
        DeviceRegistry::mountDevice().disconnect();
        DeviceRegistry::focuserDevice().disconnect();
        return {
            QStringLiteral("ConnectedDevices:Mount:false:Focuser:false"),
            QStringLiteral("ConnectAllDeviceComplete")
        };
    }

    if (command == QStringLiteral("getConnectedDevices")) {
        return connectedDevices();
    }

    if (command == QStringLiteral("loadSelectedDriverList")) {
        return selectedDriverList();
    }

    return {};
}

QStringList DeviceGatewayService::saveSerialPortSelection(const QString &payload)
{
    const QStringList parts = payload.split(QLatin1Char(':'));
    if (parts.size() != 2) {
        return {};
    }

    const QString deviceType = parts.at(0).trimmed();
    const QString rawPort = parts.at(1).trimmed();
    const bool useDefault = rawPort.isEmpty() || rawPort.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0;

    QSettings settings(QStringLiteral("QUARCS"), QStringLiteral("EmbeddedFrontend"));

    if (deviceType == QStringLiteral("Focuser")) {
        settings.setValue(QStringLiteral("bridge/FocuserSerialPort"), useDefault ? QString() : rawPort);
        settings.sync();
        DeviceRegistry::reloadFocuserDevice();
        return {
            QStringLiteral("SDKVersionAndUSBSerialPath:MainCamera:AndroidBridge:null:Guider:AndroidBridge:null:Focuser:QFocuser:%1")
                .arg(useDefault ? QStringLiteral("null") : rawPort)
        };
    }

    if (deviceType == QStringLiteral("Mount")) {
        settings.setValue(QStringLiteral("bridge/MountSerialPort"), useDefault ? QString() : rawPort);
        settings.sync();
        DeviceRegistry::reloadMountDevice();
        return {};
    }

    return {};
}

QStringList DeviceGatewayService::selectedDriverList() const
{
    const bool mountConnected = DeviceRegistry::mountDevice().state().connected;
    const bool focuserConnected = DeviceRegistry::focuserDevice().state().connected;

    return {
        QStringLiteral("SelectedDriverList:Mount:%1:%2:INDI:Focuser:%3:%4:SDK")
            .arg(mountConnected ? QStringLiteral("AndroidMount") : QString())
            .arg(QStringLiteral("false"))
            .arg(focuserConnected ? QStringLiteral("QFocuser") : QString())
            .arg(QStringLiteral("true"))
    };
}

QStringList DeviceGatewayService::connectedDevices() const
{
    const MountState mountState = DeviceRegistry::mountDevice().state();
    const FocuserState focuserState = DeviceRegistry::focuserDevice().state();

    return {
        QStringLiteral("ConnectedDevices:Mount:%1:Focuser:%2")
            .arg(mountState.connected ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(focuserState.connected ? QStringLiteral("true") : QStringLiteral("false"))
    };
}

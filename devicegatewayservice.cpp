#include "devicegatewayservice.h"

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
           command == QStringLiteral("disconnectSelectDriver");
}

QStringList DeviceGatewayService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(payload)

    if (command == QStringLiteral("connectAllDevice") ||
        command == QStringLiteral("autoConnectAllDevice") ||
        command == QStringLiteral("disconnectAllDevice")) {
        return {QStringLiteral("ConnectAllDeviceComplete")};
    }

    return {};
}

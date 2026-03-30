#ifndef DEVICEGATEWAYSERVICE_H
#define DEVICEGATEWAYSERVICE_H

#include "compatcommandservice.h"

class DeviceGatewayService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;

private:
    QStringList saveSerialPortSelection(const QString &payload);
    QStringList selectedDriverList() const;
    QStringList connectedDevices() const;
};

#endif // DEVICEGATEWAYSERVICE_H

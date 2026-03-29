#ifndef DEVICEGATEWAYSERVICE_H
#define DEVICEGATEWAYSERVICE_H

#include "compatcommandservice.h"

class DeviceGatewayService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // DEVICEGATEWAYSERVICE_H

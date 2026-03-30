#ifndef MOUNTSERVICE_H
#define MOUNTSERVICE_H

#include "compatcommandservice.h"

class IMountDevice;

class MountService : public CompatCommandService
{
public:
    MountService();

    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;

private:
    IMountDevice &device() const;
    QStringList mountSnapshot() const;
    bool parseRaDecPayload(const QString &payload, double *raHours, double *decDegrees) const;
    bool ensureConnected();
};

#endif // MOUNTSERVICE_H

#ifndef MOUNTSERVICE_H
#define MOUNTSERVICE_H

#include "compatcommandservice.h"

class MountService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // MOUNTSERVICE_H

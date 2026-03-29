#ifndef FILTERWHEELSERVICE_H
#define FILTERWHEELSERVICE_H

#include "compatcommandservice.h"

class FilterWheelService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // FILTERWHEELSERVICE_H

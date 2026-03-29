#ifndef PLATESOLVESERVICE_H
#define PLATESOLVESERVICE_H

#include "compatcommandservice.h"

class PlateSolveService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // PLATESOLVESERVICE_H

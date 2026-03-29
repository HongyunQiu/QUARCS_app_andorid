#ifndef SCHEDULESERVICE_H
#define SCHEDULESERVICE_H

#include "compatcommandservice.h"

class ScheduleService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // SCHEDULESERVICE_H

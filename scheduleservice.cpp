#include "scheduleservice.h"

bool ScheduleService::canHandle(const QString &command) const
{
    return command == QStringLiteral("getStagingScheduleData") ||
           command == QStringLiteral("StopSchedule") ||
           command == QStringLiteral("listSchedulePresets") ||
           command == QStringLiteral("loadSchedulePreset");
}

QStringList ScheduleService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(payload)

    if (command == QStringLiteral("getStagingScheduleData")) {
        return {QStringLiteral("StagingScheduleData:[]")};
    }

    return {};
}

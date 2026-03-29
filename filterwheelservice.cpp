#include "filterwheelservice.h"

bool FilterWheelService::canHandle(const QString &command) const
{
    return command == QStringLiteral("SetCFWPosition") ||
           command == QStringLiteral("CFWList") ||
           command == QStringLiteral("getCFWList");
}

QStringList FilterWheelService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(command)
    Q_UNUSED(payload)
    return {};
}

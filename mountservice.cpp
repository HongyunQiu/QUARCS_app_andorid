#include "mountservice.h"

bool MountService::canHandle(const QString &command) const
{
    return command == QStringLiteral("getMountParameters") ||
           command == QStringLiteral("MountMoveWest") ||
           command == QStringLiteral("MountMoveEast") ||
           command == QStringLiteral("MountMoveNorth") ||
           command == QStringLiteral("MountMoveSouth") ||
           command == QStringLiteral("MountMoveAbort") ||
           command == QStringLiteral("MountPark") ||
           command == QStringLiteral("MountTrack") ||
           command == QStringLiteral("MountHome") ||
           command == QStringLiteral("MountSYNC") ||
           command == QStringLiteral("SolveSYNC") ||
           command == QStringLiteral("MountGoto") ||
           command == QStringLiteral("MountSpeedSwitch");
}

QStringList MountService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(payload)

    if (command == QStringLiteral("MountTrack")) {
        return {QStringLiteral("MountTrack:false")};
    }

    return {};
}

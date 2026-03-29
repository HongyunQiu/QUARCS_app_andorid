#include "platesolveservice.h"

bool PlateSolveService::canHandle(const QString &command) const
{
    return command == QStringLiteral("SolveImage") ||
           command == QStringLiteral("startLoopSolveImage") ||
           command == QStringLiteral("stopLoopSolveImage") ||
           command == QStringLiteral("getStagingSolveResult") ||
           command == QStringLiteral("ClearSloveResultList");
}

QStringList PlateSolveService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(payload)

    if (command == QStringLiteral("getStagingSolveResult")) {
        return {QStringLiteral("StagingSolveResult:[]")};
    }

    return {};
}

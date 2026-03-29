#include "focuserservice.h"

bool FocuserService::canHandle(const QString &command) const
{
    return command == QStringLiteral("getFocuserParameters") ||
           command == QStringLiteral("focusSpeed") ||
           command == QStringLiteral("focusMove") ||
           command == QStringLiteral("AutoFocus") ||
           command == QStringLiteral("StopAutoFocus") ||
           command == QStringLiteral("AutoFocusConfirm");
}

QStringList FocuserService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(command)
    Q_UNUSED(payload)
    return {};
}

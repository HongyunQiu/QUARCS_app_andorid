#include "guiderservice.h"

bool GuiderService::canHandle(const QString &command) const
{
    return command == QStringLiteral("getGuiderStatus") ||
           command == QStringLiteral("getGuiderSwitchStatus") ||
           command == QStringLiteral("GuiderSwitch") ||
           command == QStringLiteral("GuiderLoopExpSwitch") ||
           command == QStringLiteral("GuiderExpTimeSwitch") ||
           command == QStringLiteral("ClearCalibrationData") ||
           command == QStringLiteral("PHD2Recalibrate");
}

QStringList GuiderService::handleCommand(const QString &command, const QString &payload)
{
    Q_UNUSED(payload)

    if (command == QStringLiteral("getGuiderSwitchStatus")) {
        return {QStringLiteral("GuiderSwitch:false")};
    }

    return {};
}

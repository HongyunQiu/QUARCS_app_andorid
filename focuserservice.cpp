#include "focuserservice.h"

#include "debuglogservice.h"
#include "hardware/devices/device_registry.h"
#include "hardware/devices/focuser_device_interface.h"

namespace {

bool parsePositiveInt(const QString &value, int *target)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        return false;
    }

    *target = parsed;
    return true;
}

}

FocuserService::FocuserService()
{
    device().connect();
}

bool FocuserService::canHandle(const QString &command) const
{
    return command == QStringLiteral("getFocuserParameters") ||
           command == QStringLiteral("FocuserSelfTest") ||
           command == QStringLiteral("FocuserConnect") ||
           command == QStringLiteral("FocuserDisconnect") ||
           command == QStringLiteral("focusSpeed") ||
           command == QStringLiteral("focusMove") ||
           command == QStringLiteral("focusMoveStep") ||
           command == QStringLiteral("focusMoveStop") ||
           command == QStringLiteral("getFocuserMoveState") ||
           command == QStringLiteral("AutoFocus") ||
           command == QStringLiteral("StopAutoFocus") ||
           command == QStringLiteral("AutoFocusConfirm");
}

QStringList FocuserService::handleCommand(const QString &command, const QString &payload)
{
    if (command == QStringLiteral("FocuserConnect")) {
        if (!ensureConnected()) {
            DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                                  QStringLiteral("Connect failed: %1").arg(device().state().lastError));
            return {QStringLiteral("FocuserSelfTest:FAIL:%1").arg(device().state().lastError)};
        }
        DebugLogService::instance().logManual(QStringLiteral("Focuser"), QStringLiteral("Connect success"));
        return {
            QStringLiteral("FocuserSelfTest:PASS:connected"),
            focusPosition().value(0)
        };
    }

    if (command == QStringLiteral("FocuserDisconnect")) {
        device().disconnect();
        DebugLogService::instance().logManual(QStringLiteral("Focuser"), QStringLiteral("Disconnected"));
        return {QStringLiteral("FocuserSelfTest:PASS:disconnected")};
    }

    if (command == QStringLiteral("FocuserSelfTest")) {
        if (!ensureConnected()) {
            DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                                  QStringLiteral("Self test failed: %1").arg(device().state().lastError));
            return {QStringLiteral("FocuserSelfTest:FAIL:%1").arg(device().state().lastError)};
        }

        const FocuserState state = device().state();
        DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                              QStringLiteral("Self test ok, pos=%1 target=%2 speed=%3 temp=%4")
                                                  .arg(state.position)
                                                  .arg(state.targetPosition)
                                                  .arg(state.speed)
                                                  .arg(state.temperature, 0, 'f', 1));
        return {
            QStringLiteral("FocuserSelfTest:PASS:position=%1:target=%2:speed=%3:temp=%4")
                .arg(state.position)
                .arg(state.targetPosition)
                .arg(state.speed)
                .arg(state.temperature, 0, 'f', 1),
            focusParameters().value(0),
            focusPosition().value(0)
        };
    }

    if (command == QStringLiteral("getFocuserParameters")) {
        ensureConnected();
        device().refreshState();
        QStringList responses = focusParameters();
        responses.append(focusPosition());
        return responses;
    }

    if (command == QStringLiteral("focusSpeed")) {
        if (!ensureConnected()) {
            return {};
        }

        int speed = 0;
        if (!parsePositiveInt(payload, &speed)) {
            return {};
        }

        if (!device().setSpeed(speed)) {
            return {};
        }

        DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                              QStringLiteral("Speed set to %1").arg(device().state().speed));

        return {
            QStringLiteral("FocusChangeSpeedSuccess:%1").arg(device().state().speed),
            focusPosition().value(0)
        };
    }

    if (command == QStringLiteral("focusMove")) {
        if (!ensureConnected()) {
            return {};
        }

        if (payload.compare(QStringLiteral("Left"), Qt::CaseInsensitive) == 0) {
            if (device().moveRelative(false, device().state().stepsPerClick)) {
                DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                                      QStringLiteral("Move left by %1").arg(device().state().stepsPerClick));
                return focusPosition();
            }
            return {};
        }

        if (payload.compare(QStringLiteral("Right"), Qt::CaseInsensitive) == 0) {
            if (device().moveRelative(true, device().state().stepsPerClick)) {
                DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                                      QStringLiteral("Move right by %1").arg(device().state().stepsPerClick));
                return focusPosition();
            }
            return {};
        }

        return {};
    }

    if (command == QStringLiteral("focusMoveStep")) {
        if (!ensureConnected()) {
            return {};
        }

        const QStringList parts = payload.split(QLatin1Char(':'));
        if (parts.size() != 2) {
            return {};
        }

        return moveSingleStep(parts.at(0), parts.at(1));
    }

    if (command == QStringLiteral("focusMoveStop")) {
        if (!ensureConnected()) {
            return {};
        }

        if (!device().abort()) {
            return {};
        }
        DebugLogService::instance().logManual(QStringLiteral("Focuser"), QStringLiteral("Move stop"));
        return focusPosition();
    }

    if (command == QStringLiteral("getFocuserMoveState")) {
        ensureConnected();
        device().refreshState();
        return {QStringLiteral("getFocuserMoveState:%1").arg(device().state().moving ? QStringLiteral("true")
                                                                                     : QStringLiteral("false"))};
    }

    if (command == QStringLiteral("AutoFocus") ||
        command == QStringLiteral("StopAutoFocus") ||
        command == QStringLiteral("AutoFocusConfirm")) {
        return {};
    }

    return {};
}

IFocuserDevice &FocuserService::device() const
{
    return DeviceRegistry::focuserDevice();
}

bool FocuserService::ensureConnected()
{
    if (device().state().connected) {
        return true;
    }

    return device().connect();
}

QStringList FocuserService::focusParameters() const
{
    const FocuserState state = device().state();
    return {
        QStringLiteral("FocuserParameters:%1:%2:%3:%4:%5")
            .arg(state.minPosition)
            .arg(state.maxPosition)
            .arg(state.backlash)
            .arg(state.coarseStepDivisions)
            .arg(state.stepsPerClick)
    };
}

QStringList FocuserService::focusPosition() const
{
    if (device().state().connected) {
        device().refreshState();
    }

    const FocuserState state = device().state();
    return {
        QStringLiteral("FocusPosition:%1:%2")
            .arg(state.position)
            .arg(state.targetPosition)
    };
}

QStringList FocuserService::moveSingleStep(const QString &direction, const QString &stepsText)
{
    int steps = 0;
    if (!parsePositiveInt(stepsText, &steps)) {
        return {};
    }

    if (direction.compare(QStringLiteral("Left"), Qt::CaseInsensitive) == 0) {
        if (device().moveRelative(false, steps)) {
            DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                                  QStringLiteral("Step move left %1").arg(steps));
            return focusPosition();
        }
        return {};
    }

    if (direction.compare(QStringLiteral("Right"), Qt::CaseInsensitive) == 0) {
        if (device().moveRelative(true, steps)) {
            DebugLogService::instance().logManual(QStringLiteral("Focuser"),
                                                  QStringLiteral("Step move right %1").arg(steps));
            return focusPosition();
        }
        return {};
    }

    return {};
}

#include "mountservice.h"

#include "debuglogservice.h"
#include "hardware/devices/device_registry.h"
#include "hardware/devices/mount_device_interface.h"

namespace {

QString boolSwitch(bool enabled)
{
    return enabled ? QStringLiteral("ON") : QStringLiteral("OFF");
}

}

MountService::MountService()
{
    device().connect();
}

bool MountService::canHandle(const QString &command) const
{
    return command == QStringLiteral("MountConnect") ||
           command == QStringLiteral("MountDisconnect") ||
           command == QStringLiteral("MountSelfTest") ||
           command == QStringLiteral("MountProbeRaw") ||
           command == QStringLiteral("getMountParameters") ||
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
    if (command == QStringLiteral("MountConnect")) {
        if (!ensureConnected()) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"),
                                                  QStringLiteral("Connect failed: %1").arg(device().state().lastError));
            return {QStringLiteral("MountSelfTest:FAIL:%1").arg(device().state().lastError)};
        }
        DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Connect success"));
        return {
            QStringLiteral("MountSelfTest:PASS:connected"),
            mountSnapshot().value(0)
        };
    }

    if (command == QStringLiteral("MountDisconnect")) {
        device().disconnect();
        DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Disconnected"));
        return {QStringLiteral("MountSelfTest:PASS:disconnected")};
    }

    if (command == QStringLiteral("MountSelfTest")) {
        if (!ensureConnected()) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"),
                                                  QStringLiteral("Self test failed: %1").arg(device().state().lastError));
            return {QStringLiteral("MountSelfTest:FAIL:%1").arg(device().state().lastError)};
        }

        const MountState state = device().state();
        DebugLogService::instance().logManual(
            QStringLiteral("Mount"),
            QStringLiteral("Self test ok, ra=%1 dec=%2 tracking=%3 parked=%4")
                .arg(state.raHours, 0, 'f', 4)
                .arg(state.decDegrees, 0, 'f', 4)
                .arg(state.tracking ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(state.parked ? QStringLiteral("true") : QStringLiteral("false")));
        return {
            QStringLiteral("MountSelfTest:PASS:ra=%1:dec=%2:tracking=%3:parked=%4")
                .arg(state.raHours, 0, 'f', 6)
                .arg(state.decDegrees, 0, 'f', 6)
                .arg(state.tracking ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(state.parked ? QStringLiteral("true") : QStringLiteral("false"))
        };
    }

    if (command == QStringLiteral("MountProbeRaw")) {
        const QString response = device().debugProbeCommand(payload);
        const QString lastError = device().state().lastError;
        DebugLogService::instance().logManual(
            QStringLiteral("MountProbe"),
            QStringLiteral("cmd=%1 response=%2 error=%3")
                .arg(payload, response.isEmpty() ? QStringLiteral("<empty>") : response,
                     lastError.isEmpty() ? QStringLiteral("<none>") : lastError));
        return {
            QStringLiteral("MountProbeCommand:%1").arg(payload),
            QStringLiteral("MountProbeResponse:%1")
                .arg(response.isEmpty() ? QStringLiteral("<empty>") : response),
            QStringLiteral("MountProbeError:%1")
                .arg(lastError.isEmpty() ? QStringLiteral("<none>") : lastError)
        };
    }

    if (command == QStringLiteral("getMountParameters")) {
        return mountSnapshot();
    }

    if (command == QStringLiteral("MountMoveWest")) {
        if (ensureConnected() && device().moveEastWest(false, true)) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Move west"));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountMoveEast")) {
        if (ensureConnected() && device().moveEastWest(true, true)) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Move east"));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountMoveNorth")) {
        if (ensureConnected() && device().moveNorthSouth(true, true)) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Move north"));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountMoveSouth")) {
        if (ensureConnected() && device().moveNorthSouth(false, true)) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Move south"));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountMoveAbort")) {
        if (ensureConnected() && device().abort()) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Move abort"));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountPark")) {
        if (!ensureConnected()) {
            return {};
        }
        const MountState state = device().state();
        const bool ok = state.parked ? device().unpark() : device().park();
        if (ok) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"),
                                                  state.parked ? QStringLiteral("Unpark") : QStringLiteral("Park"));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountTrack")) {
        if (!ensureConnected()) {
            return {};
        }
        const MountState state = device().state();
        if (device().setTrackingEnabled(!state.tracking)) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"),
                                                  QStringLiteral("Tracking %1").arg(!state.tracking ? QStringLiteral("on")
                                                                                                    : QStringLiteral("off")));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountHome")) {
        return {QStringLiteral("TelescopeStatus:Idle")};
    }

    if (command == QStringLiteral("MountSYNC") || command == QStringLiteral("SolveSYNC")) {
        if (!ensureConnected()) {
            return {};
        }
        const MountState state = device().state();
        if (device().syncRaDec(state.raHours, state.decDegrees)) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"), QStringLiteral("Sync current RA/DEC"));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountGoto")) {
        double raHours = 0.0;
        double decDegrees = 0.0;
        if (!parseRaDecPayload(payload, &raHours, &decDegrees)) {
            return {};
        }

        if (ensureConnected() && device().gotoRaDec(raHours, decDegrees)) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"),
                                                  QStringLiteral("Goto RA=%1 DEC=%2")
                                                      .arg(raHours, 0, 'f', 4)
                                                      .arg(decDegrees, 0, 'f', 4));
            return mountSnapshot();
        }
        return {};
    }

    if (command == QStringLiteral("MountSpeedSwitch")) {
        if (ensureConnected() && device().cycleSlewRate()) {
            DebugLogService::instance().logManual(QStringLiteral("Mount"),
                                                  QStringLiteral("Cycle slew rate to %1").arg(device().state().slewRate));
            return mountSnapshot();
        }
        return {};
    }

    return {};
}

IMountDevice &MountService::device() const
{
    return DeviceRegistry::mountDevice();
}

bool MountService::ensureConnected()
{
    if (device().state().connected) {
        return true;
    }

    return device().connect();
}

QStringList MountService::mountSnapshot() const
{
    if (device().state().connected) {
        device().refreshState();
    }

    const MountState state = device().state();
    return {
        QStringLiteral("TelescopeRADEC:%1:%2").arg(state.raHours, 0, 'f', 6).arg(state.decDegrees, 0, 'f', 6),
        QStringLiteral("TelescopeStatus:%1").arg(state.slewing ? QStringLiteral("Moving") : QStringLiteral("Idle")),
        QStringLiteral("TelescopePark:%1").arg(boolSwitch(state.parked)),
        QStringLiteral("TelescopeTrack:%1").arg(boolSwitch(state.tracking)),
        QStringLiteral("TelescopePierSide:%1").arg(state.pierSide),
        QStringLiteral("TelescopeTotalSlewRate:%1").arg(state.totalSlewRates),
        QStringLiteral("MountSetSpeedSuccess:%1").arg(state.slewRate)
    };
}

bool MountService::parseRaDecPayload(const QString &payload, double *raHours, double *decDegrees) const
{
    QString normalized = payload;
    normalized.replace(QStringLiteral("Ra:"), QString());
    normalized.replace(QStringLiteral("RA:"), QString());
    normalized.replace(QStringLiteral("Dec:"), QString());
    normalized.replace(QStringLiteral("DEC:"), QString());
    normalized.replace(QLatin1Char(','), QLatin1Char(':'));
    normalized.replace(QLatin1Char(';'), QLatin1Char(':'));

    const QStringList parts = normalized.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (parts.size() != 2) {
        return false;
    }

    bool raOk = false;
    bool decOk = false;
    const double parsedRa = parts.at(0).trimmed().toDouble(&raOk);
    const double parsedDec = parts.at(1).trimmed().toDouble(&decOk);
    if (!raOk || !decOk) {
        return false;
    }

    *raHours = parsedRa;
    *decDegrees = parsedDec;
    return true;
}

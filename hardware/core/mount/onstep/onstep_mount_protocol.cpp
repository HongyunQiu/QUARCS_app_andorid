#include "hardware/core/mount/onstep/onstep_mount_protocol.h"

#include <QStringList>
#include <QtGlobal>

namespace {

QString trimResponse(const QByteArray &response)
{
    return QString::fromLatin1(response).trimmed();
}

QString formatRa(double raHours)
{
    double normalized = raHours;
    while (normalized < 0.0) {
        normalized += 24.0;
    }
    while (normalized >= 24.0) {
        normalized -= 24.0;
    }

    const int totalSeconds = qRound(normalized * 3600.0);
    const int hours = (totalSeconds / 3600) % 24;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString formatDec(double decDegrees)
{
    const double bounded = qBound(-90.0, decDegrees, 90.0);
    const int sign = bounded < 0.0 ? -1 : 1;
    const int totalArcSeconds = qRound(qAbs(bounded) * 3600.0);
    const int degrees = totalArcSeconds / 3600;
    const int minutes = (totalArcSeconds / 60) % 60;
    const int seconds = totalArcSeconds % 60;
    return QStringLiteral("%1%2*%3:%4")
        .arg(sign < 0 ? QLatin1Char('-') : QLatin1Char('+'))
        .arg(degrees, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

bool parseSexagesimal(const QString &text, bool isRa, double *value)
{
    QString normalized = text.trimmed();
    normalized.replace(QLatin1Char('*'), QLatin1Char(':'));
    const QStringList parts = normalized.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    bool ok0 = false;
    bool ok1 = false;
    bool ok2 = true;
    const double a = parts.at(0).toDouble(&ok0);
    const double b = parts.at(1).toDouble(&ok1);
    double c = 0.0;
    if (parts.size() >= 3) {
        c = parts.at(2).toDouble(&ok2);
    }
    if (!ok0 || !ok1 || !ok2) {
        return false;
    }

    if (isRa) {
        *value = a + (b / 60.0) + (c / 3600.0);
        return true;
    }

    const double sign = normalized.startsWith(QLatin1Char('-')) ? -1.0 : 1.0;
    *value = sign * (qAbs(a) + (b / 60.0) + (c / 3600.0));
    return true;
}

}

QByteArray OnStepMountProtocol::buildHandshake()
{
    return QByteArrayLiteral(":GVP#");
}

QByteArray OnStepMountProtocol::buildGetRa()
{
    return QByteArrayLiteral(":GR#");
}

QByteArray OnStepMountProtocol::buildGetDec()
{
    return QByteArrayLiteral(":GD#");
}

QByteArray OnStepMountProtocol::buildGetStatus()
{
    return QByteArrayLiteral(":GU#");
}

QByteArray OnStepMountProtocol::buildSetTargetRa(double raHours)
{
    return QStringLiteral(":Sr%1#").arg(formatRa(raHours)).toLatin1();
}

QByteArray OnStepMountProtocol::buildSetTargetDec(double decDegrees)
{
    return QStringLiteral(":Sd%1#").arg(formatDec(decDegrees)).toLatin1();
}

QByteArray OnStepMountProtocol::buildGoto()
{
    return QByteArrayLiteral(":MS#");
}

QByteArray OnStepMountProtocol::buildSync()
{
    return QByteArrayLiteral(":CM#");
}

QByteArray OnStepMountProtocol::buildAbort()
{
    return QByteArrayLiteral(":Q#");
}

QByteArray OnStepMountProtocol::buildSetTracking(bool enabled)
{
    return enabled ? QByteArrayLiteral(":Te#") : QByteArrayLiteral(":Td#");
}

QByteArray OnStepMountProtocol::buildMoveNorthSouth(bool north)
{
    return north ? QByteArrayLiteral(":Mn#") : QByteArrayLiteral(":Ms#");
}

QByteArray OnStepMountProtocol::buildMoveEastWest(bool east)
{
    return east ? QByteArrayLiteral(":Me#") : QByteArrayLiteral(":Mw#");
}

QByteArray OnStepMountProtocol::buildPark()
{
    return QByteArrayLiteral(":hP#");
}

QByteArray OnStepMountProtocol::buildUnpark()
{
    return QByteArrayLiteral(":hR#");
}

QByteArray OnStepMountProtocol::buildSetSlewRate(int slewRate)
{
    const int bounded = qBound(0, slewRate, 9);
    return QStringLiteral(":R%1#").arg(bounded).toLatin1();
}

bool OnStepMountProtocol::parseRa(const QByteArray &response, double *raHours)
{
    return parseSexagesimal(trimResponse(response), true, raHours);
}

bool OnStepMountProtocol::parseDec(const QByteArray &response, double *decDegrees)
{
    return parseSexagesimal(trimResponse(response), false, decDegrees);
}

bool OnStepMountProtocol::parseStatus(const QByteArray &response, OnStepMountStatus *status)
{
    if (!status) {
        return false;
    }

    const QString raw = trimResponse(response);
    if (raw.isEmpty()) {
        return false;
    }

    status->raw = raw;
    status->parked = raw.contains(QLatin1Char('P'));
    const bool hasUpperN = raw.contains(QLatin1Char('N'));
    const bool hasLowerN = raw.contains(QLatin1Char('n'));
    const bool parkInProgress = raw.contains(QLatin1Char('I'));
    const bool idle = hasUpperN && hasLowerN;
    const bool tracking = hasUpperN && !hasLowerN;
    const bool slewing = !idle && !tracking && (!status->parked || parkInProgress);
    status->tracking = !status->parked && tracking;
    status->slewing = slewing;
    return true;
}

bool OnStepMountProtocol::parseSingleCharSuccess(const QByteArray &response)
{
    const QString raw = trimResponse(response);
    return !raw.isEmpty() && raw.at(0) == QLatin1Char('1');
}

int OnStepMountProtocol::parseGotoErrorCode(const QByteArray &response)
{
    const QString raw = trimResponse(response);
    if (raw.isEmpty()) {
        return -1;
    }

    bool ok = false;
    const int code = raw.left(1).toInt(&ok);
    return ok ? code : -1;
}

bool OnStepMountProtocol::parseSyncSuccess(const QByteArray &response)
{
    const QString raw = trimResponse(response);
    return !raw.isEmpty() && !raw.startsWith(QLatin1Char('E'));
}

#include "hardware/core/focuser/qfocuser/qfocuser_protocol.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace {

QByteArray buildCommand(int cmdId, const QJsonObject &payload = QJsonObject())
{
    QJsonObject object = payload;
    object.insert(QStringLiteral("cmd_id"), cmdId);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}

QByteArray QFocuserProtocol::buildGetVersion()
{
    return buildCommand(1);
}

QByteArray QFocuserProtocol::buildGetTemperature()
{
    return buildCommand(4);
}

QByteArray QFocuserProtocol::buildGetPosition()
{
    return buildCommand(5);
}

QByteArray QFocuserProtocol::buildMoveRelative(bool outward, int steps)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("dir"), outward ? 1 : -1);
    payload.insert(QStringLiteral("step"), steps);
    return buildCommand(2, payload);
}

QByteArray QFocuserProtocol::buildMoveAbsolute(int position)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("tar"), position);
    return buildCommand(6, payload);
}

QByteArray QFocuserProtocol::buildAbort()
{
    return buildCommand(3);
}

QByteArray QFocuserProtocol::buildSetReverse(bool enabled)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("rev"), enabled ? 1 : 0);
    return buildCommand(7, payload);
}

QByteArray QFocuserProtocol::buildSyncPosition(int position)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("init_val"), position);
    return buildCommand(11, payload);
}

QByteArray QFocuserProtocol::buildSetSpeed(int speed)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("speed"), 4 - speed);
    return buildCommand(13, payload);
}

QByteArray QFocuserProtocol::buildSetHold()
{
    QJsonObject payload;
    payload.insert(QStringLiteral("ihold"), 0);
    payload.insert(QStringLiteral("irun"), 5);
    return buildCommand(16, payload);
}

bool QFocuserProtocol::tryExtractJsonObjects(QByteArray *buffer, QList<QJsonObject> *objects)
{
    bool extractedAny = false;
    while (true) {
        const int endIndex = buffer->indexOf('}');
        if (endIndex < 0) {
            return extractedAny;
        }

        const QByteArray one = buffer->left(endIndex + 1);
        buffer->remove(0, endIndex + 1);

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(one, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }

        objects->append(document.object());
        extractedAny = true;
    }
}

bool QFocuserProtocol::parseVersion(const QJsonObject &object, QFocuserVersionInfo *version)
{
    if (object.value(QStringLiteral("idx")).toInt() != 1 ||
        !object.contains(QStringLiteral("version")) ||
        !object.contains(QStringLiteral("bv"))) {
        return false;
    }

    version->version = object.value(QStringLiteral("version")).toInt();
    version->boardVersion = object.value(QStringLiteral("bv")).toInt();
    version->deviceId = object.value(QStringLiteral("id")).toString();
    return true;
}

bool QFocuserProtocol::parseTelemetry(const QJsonObject &object, QFocuserTelemetry *telemetry)
{
    if (object.value(QStringLiteral("idx")).toInt() != 4 ||
        !object.contains(QStringLiteral("o_t")) ||
        !object.contains(QStringLiteral("c_t")) ||
        !object.contains(QStringLiteral("c_r"))) {
        return false;
    }

    telemetry->outTempC = object.value(QStringLiteral("o_t")).toDouble() / 1000.0;
    telemetry->chipTempC = object.value(QStringLiteral("c_t")).toDouble() / 1000.0;
    telemetry->voltageV = object.value(QStringLiteral("c_r")).toDouble() / 10.0;
    return true;
}

bool QFocuserProtocol::parsePosition(const QJsonObject &object, int *position)
{
    if (object.value(QStringLiteral("idx")).toInt() != 5 ||
        !object.contains(QStringLiteral("pos"))) {
        return false;
    }

    *position = object.value(QStringLiteral("pos")).toInt();
    return true;
}

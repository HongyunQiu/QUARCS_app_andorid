#ifndef QFOCUSER_PROTOCOL_H
#define QFOCUSER_PROTOCOL_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

struct QFocuserVersionInfo {
    int version = 0;
    int boardVersion = 0;
    QString deviceId;
};

struct QFocuserTelemetry {
    double outTempC = 0.0;
    double chipTempC = 0.0;
    double voltageV = 0.0;
};

class QFocuserProtocol
{
public:
    static QByteArray buildGetVersion();
    static QByteArray buildGetTemperature();
    static QByteArray buildGetPosition();
    static QByteArray buildMoveRelative(bool outward, int steps);
    static QByteArray buildMoveAbsolute(int position);
    static QByteArray buildAbort();
    static QByteArray buildSetReverse(bool enabled);
    static QByteArray buildSyncPosition(int position);
    static QByteArray buildSetSpeed(int speed);
    static QByteArray buildSetHold();

    static bool tryExtractJsonObjects(QByteArray *buffer, QList<QJsonObject> *objects);
    static bool parseVersion(const QJsonObject &object, QFocuserVersionInfo *version);
    static bool parseTelemetry(const QJsonObject &object, QFocuserTelemetry *telemetry);
    static bool parsePosition(const QJsonObject &object, int *position);
};

#endif // QFOCUSER_PROTOCOL_H

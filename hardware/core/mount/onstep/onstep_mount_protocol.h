#ifndef ONSTEP_MOUNT_PROTOCOL_H
#define ONSTEP_MOUNT_PROTOCOL_H

#include <QByteArray>
#include <QString>

struct OnStepMountStatus {
    bool parked = false;
    bool slewing = false;
    bool tracking = false;
    QString raw;
};

class OnStepMountProtocol
{
public:
    static QByteArray buildHandshake();
    static QByteArray buildGetRa();
    static QByteArray buildGetDec();
    static QByteArray buildGetStatus();
    static QByteArray buildSetTargetRa(double raHours);
    static QByteArray buildSetTargetDec(double decDegrees);
    static QByteArray buildGoto();
    static QByteArray buildSync();
    static QByteArray buildAbort();
    static QByteArray buildSetTracking(bool enabled);
    static QByteArray buildMoveNorthSouth(bool north);
    static QByteArray buildMoveEastWest(bool east);
    static QByteArray buildPark();
    static QByteArray buildUnpark();
    static QByteArray buildSetSlewRate(int slewRate);

    static bool parseRa(const QByteArray &response, double *raHours);
    static bool parseDec(const QByteArray &response, double *decDegrees);
    static bool parseStatus(const QByteArray &response, OnStepMountStatus *status);
    static bool parseSingleCharSuccess(const QByteArray &response);
    static int parseGotoErrorCode(const QByteArray &response);
    static bool parseSyncSuccess(const QByteArray &response);
};

#endif // ONSTEP_MOUNT_PROTOCOL_H

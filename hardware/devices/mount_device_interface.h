#ifndef MOUNT_DEVICE_INTERFACE_H
#define MOUNT_DEVICE_INTERFACE_H

#include <QString>

struct MountState {
    bool connected = false;
    bool slewing = false;
    bool tracking = false;
    bool parked = false;
    double raHours = 0.0;
    double decDegrees = 0.0;
    double targetRaHours = 0.0;
    double targetDecDegrees = 0.0;
    int slewRate = 1;
    int totalSlewRates = 5;
    QString pierSide = QStringLiteral("UNKNOWN");
    QString lastError;
};

class IMountDevice
{
public:
    virtual ~IMountDevice() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual MountState state() const = 0;
    virtual bool refreshState() = 0;
    virtual bool gotoRaDec(double raHours, double decDegrees) = 0;
    virtual bool syncRaDec(double raHours, double decDegrees) = 0;
    virtual bool park() = 0;
    virtual bool unpark() = 0;
    virtual bool abort() = 0;
    virtual bool setTrackingEnabled(bool enabled) = 0;
    virtual bool moveNorthSouth(bool north, bool start) = 0;
    virtual bool moveEastWest(bool east, bool start) = 0;
    virtual bool cycleSlewRate() = 0;
    virtual QString debugProbeCommand(const QString &command) = 0;
};

#endif // MOUNT_DEVICE_INTERFACE_H

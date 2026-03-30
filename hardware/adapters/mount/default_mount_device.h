#ifndef DEFAULT_MOUNT_DEVICE_H
#define DEFAULT_MOUNT_DEVICE_H

#include "hardware/devices/mount_device_interface.h"

class DefaultMountDevice : public IMountDevice
{
public:
    bool connect() override;
    void disconnect() override;
    MountState state() const override;
    bool refreshState() override;
    bool gotoRaDec(double raHours, double decDegrees) override;
    bool syncRaDec(double raHours, double decDegrees) override;
    bool park() override;
    bool unpark() override;
    bool abort() override;
    bool setTrackingEnabled(bool enabled) override;
    bool moveNorthSouth(bool north, bool start) override;
    bool moveEastWest(bool east, bool start) override;
    bool cycleSlewRate() override;
    QString debugProbeCommand(const QString &command) override;

private:
    void ensureConnected();
    void clampCoordinates();

    MountState m_state;
};

#endif // DEFAULT_MOUNT_DEVICE_H

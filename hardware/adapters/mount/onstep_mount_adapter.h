#ifndef ONSTEP_MOUNT_ADAPTER_H
#define ONSTEP_MOUNT_ADAPTER_H

#include "hardware/devices/mount_device_interface.h"

#include <memory>

class ITransport;
class OnStepMountCore;

class OnStepMountAdapter : public IMountDevice
{
public:
    explicit OnStepMountAdapter(std::unique_ptr<ITransport> transport);
    ~OnStepMountAdapter() override;

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
    std::unique_ptr<OnStepMountCore> m_core;
};

#endif // ONSTEP_MOUNT_ADAPTER_H

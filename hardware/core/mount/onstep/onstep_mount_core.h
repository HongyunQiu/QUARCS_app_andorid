#ifndef ONSTEP_MOUNT_CORE_H
#define ONSTEP_MOUNT_CORE_H

#include "hardware/core/mount/onstep/onstep_mount_protocol.h"
#include "hardware/devices/mount_device_interface.h"

#include <memory>

class ITransport;

class OnStepMountCore
{
public:
    explicit OnStepMountCore(std::unique_ptr<ITransport> transport, int timeoutMs = 1200);

    bool connect();
    void disconnect();
    bool isConnected() const;

    MountState state() const;
    bool refreshState();
    bool handshake();
    bool gotoRaDec(double raHours, double decDegrees);
    bool syncRaDec(double raHours, double decDegrees);
    bool park();
    bool unpark();
    bool abort();
    bool setTrackingEnabled(bool enabled);
    bool moveNorthSouth(bool north, bool start);
    bool moveEastWest(bool east, bool start);
    bool setSlewRate(int slewRate);
    QString debugProbeCommand(const QString &command);

private:
    bool simulateIfNeeded();
    bool ensureTransportOpen();
    bool refreshCoordinates();
    bool refreshStatus();
    bool queryTerminated(const QByteArray &command, QByteArray *response);
    bool queryFlexible(const QByteArray &command, QByteArray *response, int settleMs = 80);
    bool writeBlind(const QByteArray &command);
    void applySimulationDefaults();
    void clampCoordinates();
    void setError(const QString &message);

    std::unique_ptr<ITransport> m_transport;
    int m_timeoutMs = 1200;
    bool m_simulationMode = false;
    QByteArray m_rxBuffer;
    MountState m_state;
};

#endif // ONSTEP_MOUNT_CORE_H

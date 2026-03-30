#include "hardware/adapters/mount/onstep_mount_adapter.h"

#include "hardware/core/mount/onstep/onstep_mount_core.h"
#include "hardware/transports/transport_interface.h"

OnStepMountAdapter::OnStepMountAdapter(std::unique_ptr<ITransport> transport)
    : m_core(new OnStepMountCore(std::move(transport)))
{
}

OnStepMountAdapter::~OnStepMountAdapter() = default;

bool OnStepMountAdapter::connect()
{
    return m_core->connect();
}

void OnStepMountAdapter::disconnect()
{
    m_core->disconnect();
}

MountState OnStepMountAdapter::state() const
{
    return m_core->state();
}

bool OnStepMountAdapter::refreshState()
{
    return m_core->refreshState();
}

bool OnStepMountAdapter::gotoRaDec(double raHours, double decDegrees)
{
    return m_core->gotoRaDec(raHours, decDegrees);
}

bool OnStepMountAdapter::syncRaDec(double raHours, double decDegrees)
{
    return m_core->syncRaDec(raHours, decDegrees);
}

bool OnStepMountAdapter::park()
{
    return m_core->park();
}

bool OnStepMountAdapter::unpark()
{
    return m_core->unpark();
}

bool OnStepMountAdapter::abort()
{
    return m_core->abort();
}

bool OnStepMountAdapter::setTrackingEnabled(bool enabled)
{
    return m_core->setTrackingEnabled(enabled);
}

bool OnStepMountAdapter::moveNorthSouth(bool north, bool start)
{
    return m_core->moveNorthSouth(north, start);
}

bool OnStepMountAdapter::moveEastWest(bool east, bool start)
{
    return m_core->moveEastWest(east, start);
}

bool OnStepMountAdapter::cycleSlewRate()
{
    MountState current = m_core->state();
    int nextRate = current.slewRate + 1;
    if (nextRate >= current.totalSlewRates) {
        nextRate = 0;
    }
    return m_core->setSlewRate(nextRate);
}

QString OnStepMountAdapter::debugProbeCommand(const QString &command)
{
    return m_core->debugProbeCommand(command);
}

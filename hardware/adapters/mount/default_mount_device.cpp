#include "hardware/adapters/mount/default_mount_device.h"

#include <QtGlobal>

namespace {

constexpr double kRaStepHours = 0.02;
constexpr double kDecStepDegrees = 0.5;

}

bool DefaultMountDevice::connect()
{
    ensureConnected();
    return true;
}

void DefaultMountDevice::disconnect()
{
    m_state = MountState();
}

MountState DefaultMountDevice::state() const
{
    return m_state;
}

bool DefaultMountDevice::refreshState()
{
    ensureConnected();
    if (m_state.slewing) {
        m_state.raHours = m_state.targetRaHours;
        m_state.decDegrees = m_state.targetDecDegrees;
        m_state.slewing = false;
    }
    clampCoordinates();
    return true;
}

bool DefaultMountDevice::gotoRaDec(double raHours, double decDegrees)
{
    ensureConnected();
    m_state.targetRaHours = raHours;
    m_state.targetDecDegrees = decDegrees;
    m_state.raHours = raHours;
    m_state.decDegrees = decDegrees;
    m_state.slewing = false;
    clampCoordinates();
    return true;
}

bool DefaultMountDevice::syncRaDec(double raHours, double decDegrees)
{
    ensureConnected();
    m_state.raHours = raHours;
    m_state.decDegrees = decDegrees;
    m_state.targetRaHours = raHours;
    m_state.targetDecDegrees = decDegrees;
    clampCoordinates();
    return true;
}

bool DefaultMountDevice::park()
{
    ensureConnected();
    m_state.parked = true;
    m_state.slewing = false;
    m_state.tracking = false;
    return true;
}

bool DefaultMountDevice::unpark()
{
    ensureConnected();
    m_state.parked = false;
    return true;
}

bool DefaultMountDevice::abort()
{
    ensureConnected();
    m_state.slewing = false;
    return true;
}

bool DefaultMountDevice::setTrackingEnabled(bool enabled)
{
    ensureConnected();
    if (m_state.parked && enabled) {
        m_state.lastError = QStringLiteral("Cannot enable tracking while parked");
        return false;
    }

    m_state.tracking = enabled;
    return true;
}

bool DefaultMountDevice::moveNorthSouth(bool north, bool start)
{
    ensureConnected();
    if (!start) {
        m_state.slewing = false;
        return true;
    }

    m_state.slewing = true;
    m_state.decDegrees += north ? kDecStepDegrees : -kDecStepDegrees;
    clampCoordinates();
    return true;
}

bool DefaultMountDevice::moveEastWest(bool east, bool start)
{
    ensureConnected();
    if (!start) {
        m_state.slewing = false;
        return true;
    }

    m_state.slewing = true;
    m_state.raHours += east ? kRaStepHours : -kRaStepHours;
    clampCoordinates();
    return true;
}

bool DefaultMountDevice::cycleSlewRate()
{
    ensureConnected();
    if (m_state.totalSlewRates <= 0) {
        m_state.lastError = QStringLiteral("No slew rates available");
        return false;
    }

    ++m_state.slewRate;
    if (m_state.slewRate > m_state.totalSlewRates) {
        m_state.slewRate = 1;
    }
    return true;
}

QString DefaultMountDevice::debugProbeCommand(const QString &command)
{
    ensureConnected();
    Q_UNUSED(command)
    return QStringLiteral("simulation");
}

void DefaultMountDevice::ensureConnected()
{
    if (!m_state.connected) {
        m_state.connected = true;
        m_state.tracking = true;
        m_state.pierSide = QStringLiteral("EAST");
    }
}

void DefaultMountDevice::clampCoordinates()
{
    while (m_state.raHours < 0.0) {
        m_state.raHours += 24.0;
    }
    while (m_state.raHours >= 24.0) {
        m_state.raHours -= 24.0;
    }

    m_state.decDegrees = qBound(-90.0, m_state.decDegrees, 90.0);
}

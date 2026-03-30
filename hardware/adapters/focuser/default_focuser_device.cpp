#include "hardware/adapters/focuser/default_focuser_device.h"

#include <QtGlobal>

bool DefaultFocuserDevice::connect()
{
    ensureConnected();
    return true;
}

void DefaultFocuserDevice::disconnect()
{
    m_state = FocuserState();
}

FocuserState DefaultFocuserDevice::state() const
{
    return m_state;
}

bool DefaultFocuserDevice::refreshState()
{
    ensureConnected();
    if (m_state.moving) {
        m_state.position = m_state.targetPosition;
        m_state.moving = false;
    }
    return true;
}

bool DefaultFocuserDevice::moveAbsolute(int position)
{
    ensureConnected();
    m_state.targetPosition = boundedPosition(position);
    m_state.moving = true;
    return true;
}

bool DefaultFocuserDevice::moveRelative(bool outward, int steps)
{
    ensureConnected();
    m_state.moving = true;

    const int direction = outward ? 1 : -1;
    const int signedSteps = m_state.reversed ? -steps : steps;
    m_state.targetPosition = boundedPosition(m_state.position + (direction * signedSteps));
    return true;
}

bool DefaultFocuserDevice::abort()
{
    ensureConnected();
    m_state.moving = false;
    m_state.targetPosition = m_state.position;
    return true;
}

bool DefaultFocuserDevice::syncPosition(int position)
{
    ensureConnected();
    m_state.position = boundedPosition(position);
    m_state.targetPosition = m_state.position;
    return true;
}

bool DefaultFocuserDevice::setSpeed(int speed)
{
    ensureConnected();
    m_state.speed = qMax(0, speed);
    return true;
}

bool DefaultFocuserDevice::setReverse(bool enabled)
{
    ensureConnected();
    m_state.reversed = enabled;
    return true;
}

void DefaultFocuserDevice::ensureConnected()
{
    if (!m_state.connected) {
        m_state.connected = true;
        m_state.position = 5000;
        m_state.targetPosition = 5000;
        m_state.temperature = 20.0;
    }
}

int DefaultFocuserDevice::boundedPosition(int position) const
{
    return qBound(m_state.minPosition, position, m_state.maxPosition);
}

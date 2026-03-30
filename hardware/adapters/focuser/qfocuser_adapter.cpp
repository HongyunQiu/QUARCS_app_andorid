#include "hardware/adapters/focuser/qfocuser_adapter.h"

#include "hardware/core/focuser/qfocuser/qfocuser_core.h"
#include "hardware/transports/transport_interface.h"

QFocuserAdapter::QFocuserAdapter(std::unique_ptr<ITransport> transport)
    : m_core(new QFocuserCore(std::move(transport)))
{
}

QFocuserAdapter::~QFocuserAdapter() = default;

bool QFocuserAdapter::connect()
{
    return m_core->connect();
}

void QFocuserAdapter::disconnect()
{
    m_core->disconnect();
}

FocuserState QFocuserAdapter::state() const
{
    return m_core->state();
}

bool QFocuserAdapter::refreshState()
{
    return m_core->refreshState();
}

bool QFocuserAdapter::moveAbsolute(int position)
{
    return m_core->moveAbsolute(position);
}

bool QFocuserAdapter::moveRelative(bool outward, int steps)
{
    return m_core->moveRelative(outward, steps);
}

bool QFocuserAdapter::abort()
{
    return m_core->abort();
}

bool QFocuserAdapter::syncPosition(int position)
{
    return m_core->syncPosition(position);
}

bool QFocuserAdapter::setSpeed(int speed)
{
    return m_core->setSpeed(speed);
}

bool QFocuserAdapter::setReverse(bool enabled)
{
    return m_core->setReverse(enabled);
}

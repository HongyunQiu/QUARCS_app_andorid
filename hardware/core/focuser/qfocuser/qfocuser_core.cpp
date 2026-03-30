#include "hardware/core/focuser/qfocuser/qfocuser_core.h"

#include "hardware/transports/transport_interface.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QtMath>
#include <QtGlobal>

QFocuserCore::QFocuserCore(std::unique_ptr<ITransport> transport, int timeoutMs)
    : m_transport(std::move(transport))
    , m_timeoutMs(timeoutMs)
{
    m_state.minPosition = -64000;
    m_state.maxPosition = 64000;
    m_state.stepsPerClick = 50;
    m_state.coarseStepDivisions = 10;
}

bool QFocuserCore::connect()
{
    if (!m_transport) {
        applySimulationDefaults();
        return true;
    }

    if (!m_transport->open()) {
        m_simulationMode = false;
        setError(QStringLiteral("Failed to open focuser transport"));
        return false;
    }

    m_simulationMode = false;
    m_state.connected = true;
    return handshake() && refreshPosition();
}

void QFocuserCore::disconnect()
{
    if (m_transport) {
        m_transport->close();
    }

    m_rxBuffer.clear();
    m_state.connected = false;
    m_state.moving = false;
    m_state.lastError.clear();
}

bool QFocuserCore::isConnected() const
{
    return m_state.connected;
}

FocuserState QFocuserCore::state() const
{
    return m_state;
}

bool QFocuserCore::refreshState()
{
    if (simulateIfNeeded()) {
        if (m_state.moving) {
            m_state.position = m_state.targetPosition;
            m_state.moving = false;
        }
        return true;
    }

    if (!refreshPosition()) {
        return false;
    }

    if (m_state.moving && m_state.position == m_state.targetPosition) {
        m_state.moving = false;
    }

    return true;
}

bool QFocuserCore::handshake()
{
    if (simulateIfNeeded()) {
        return true;
    }

    QJsonObject response;
    if (!transact(QFocuserProtocol::buildGetVersion(), 1, &response)) {
        return false;
    }

    QFocuserProtocol::parseVersion(response, &m_version);

    if (!transact(QFocuserProtocol::buildGetTemperature(), 4, &response)) {
        return false;
    }

    if (QFocuserProtocol::parseTelemetry(response, &m_telemetry)) {
        m_state.temperature = m_telemetry.outTempC;
    }

    if (m_telemetry.voltageV <= 0.0) {
        transact(QFocuserProtocol::buildSetHold(), 16, nullptr);
    }

    return true;
}

bool QFocuserCore::refreshPosition()
{
    if (simulateIfNeeded()) {
        return true;
    }

    QJsonObject response;
    if (!transact(QFocuserProtocol::buildGetPosition(), 5, &response)) {
        return false;
    }

    int position = m_state.position;
    if (QFocuserProtocol::parsePosition(response, &position)) {
        m_state.position = position;
        m_state.targetPosition = position;
        return true;
    }

    setError(QStringLiteral("Failed to parse focuser position"));
    return false;
}

bool QFocuserCore::moveAbsolute(int position)
{
    if (simulateIfNeeded()) {
        m_state.targetPosition = qBound(m_state.minPosition, position, m_state.maxPosition);
        m_state.position = m_state.targetPosition;
        m_state.moving = false;
        return true;
    }

    const int bounded = qBound(m_state.minPosition, position, m_state.maxPosition);
    if (!transact(QFocuserProtocol::buildMoveAbsolute(bounded), 6, nullptr)) {
        return false;
    }

    m_state.targetPosition = bounded;
    m_state.moving = true;
    return true;
}

bool QFocuserCore::moveRelative(bool outward, int steps)
{
    if (simulateIfNeeded()) {
        const int direction = outward ? 1 : -1;
        const int target = m_state.position + (direction * (m_state.reversed ? -steps : steps));
        return moveAbsolute(target);
    }

    if (!refreshPosition()) {
        return false;
    }

    const int direction = outward ? 1 : -1;
    const int target = m_state.position + (direction * (m_state.reversed ? -steps : steps));
    return moveAbsolute(qBound(m_state.minPosition, target, m_state.maxPosition));
}

bool QFocuserCore::abort()
{
    if (simulateIfNeeded()) {
        m_state.moving = false;
        m_state.targetPosition = m_state.position;
        return true;
    }

    if (!transact(QFocuserProtocol::buildAbort(), 3, nullptr)) {
        return false;
    }

    refreshPosition();
    m_state.moving = false;
    m_state.targetPosition = m_state.position;
    return true;
}

bool QFocuserCore::syncPosition(int position)
{
    if (simulateIfNeeded()) {
        const int bounded = qBound(m_state.minPosition, position, m_state.maxPosition);
        m_state.position = bounded;
        m_state.targetPosition = bounded;
        return true;
    }

    const int bounded = qBound(m_state.minPosition, position, m_state.maxPosition);
    if (!transact(QFocuserProtocol::buildSyncPosition(bounded), 11, nullptr)) {
        return false;
    }

    m_state.position = bounded;
    m_state.targetPosition = bounded;
    return true;
}

bool QFocuserCore::setSpeed(int speed)
{
    const int normalizedSpeed = qBound(0, speed, 8);
    if (simulateIfNeeded()) {
        m_state.speed = normalizedSpeed;
        return true;
    }

    if (!transact(QFocuserProtocol::buildSetSpeed(normalizedSpeed), 13, nullptr)) {
        return false;
    }

    m_state.speed = normalizedSpeed;
    return true;
}

bool QFocuserCore::setReverse(bool enabled)
{
    if (simulateIfNeeded()) {
        m_state.reversed = enabled;
        return true;
    }

    if (!transact(QFocuserProtocol::buildSetReverse(enabled), 7, nullptr)) {
        return false;
    }

    m_state.reversed = enabled;
    return true;
}

bool QFocuserCore::simulateIfNeeded()
{
    if (!m_transport || m_simulationMode) {
        applySimulationDefaults();
        return true;
    }
    return false;
}

bool QFocuserCore::transact(const QByteArray &command, int expectedIdx, QJsonObject *response)
{
    if (!m_transport || !m_transport->isOpen()) {
        setError(QStringLiteral("Transport is not open"));
        return false;
    }

    if (!m_transport->write(command)) {
        setError(QStringLiteral("Failed to write focuser command"));
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < m_timeoutMs) {
        QList<QJsonObject> objects;
        const QByteArray chunk = m_transport->read(50);
        if (!chunk.isEmpty()) {
            m_rxBuffer.append(chunk);
        }

        QFocuserProtocol::tryExtractJsonObjects(&m_rxBuffer, &objects);
        for (const QJsonObject &object : objects) {
            const int idx = object.value(QStringLiteral("idx")).toInt();
            if (idx == expectedIdx || idx == -1) {
                if (response) {
                    *response = object;
                }
                m_state.connected = true;
                m_state.lastError.clear();
                return true;
            }
        }
    }

    setError(QStringLiteral("Timed out waiting for focuser response"));
    return false;
}

void QFocuserCore::applySimulationDefaults()
{
    m_simulationMode = true;
    m_state.connected = true;
    if (m_state.targetPosition == 0 && m_state.position == 0) {
        m_state.position = 5000;
        m_state.targetPosition = 5000;
    }
    if (m_state.temperature == 0.0) {
        m_state.temperature = 20.0;
    }
    m_state.lastError.clear();
}

void QFocuserCore::setError(const QString &message)
{
    m_state.connected = false;
    m_state.lastError = message;
}

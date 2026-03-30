#include "hardware/core/mount/onstep/onstep_mount_core.h"

#include "hardware/transports/transport_interface.h"

#include <QElapsedTimer>
#include <QtGlobal>

namespace {

QString gotoErrorMessage(int code)
{
    switch (code) {
    case 0:
        return QString();
    case 1:
        return QStringLiteral("Below the horizon limit");
    case 2:
        return QStringLiteral("Above overhead limit");
    case 3:
        return QStringLiteral("Controller in standby");
    case 4:
        return QStringLiteral("Mount is parked");
    case 5:
        return QStringLiteral("Goto already in progress");
    case 6:
        return QStringLiteral("Outside mount limits");
    case 7:
        return QStringLiteral("Hardware fault");
    case 8:
        return QStringLiteral("Mount already in motion");
    case 9:
        return QStringLiteral("Unspecified mount error");
    default:
        return QStringLiteral("Unexpected goto response");
    }
}

QString normalizeHandshake(const QByteArray &response)
{
    QString normalized;
    normalized.reserve(response.size());
    for (const char ch : response) {
        const QChar qc = QLatin1Char(ch);
        if (qc.isLetterOrNumber()) {
            normalized.append(qc.toLower());
        }
    }
    return normalized;
}

}

OnStepMountCore::OnStepMountCore(std::unique_ptr<ITransport> transport, int timeoutMs)
    : m_transport(std::move(transport))
    , m_timeoutMs(timeoutMs)
{
    m_state.totalSlewRates = 10;
    m_state.slewRate = 5;
}

bool OnStepMountCore::connect()
{
    if (!m_transport) {
        applySimulationDefaults();
        return true;
    }

    if (!m_transport->open()) {
        m_simulationMode = false;
        setError(QStringLiteral("Failed to open mount transport"));
        return false;
    }

    m_simulationMode = false;
    m_state.connected = true;
    if (!handshake()) {
        return false;
    }

    return refreshState();
}

QString OnStepMountCore::debugProbeCommand(const QString &command)
{
    QString normalized = command.trimmed();
    if (normalized.isEmpty()) {
        setError(QStringLiteral("Probe command is empty"));
        return QString();
    }

    if (!normalized.endsWith(QLatin1Char('#'))) {
        normalized.append(QLatin1Char('#'));
    }

    if (simulateIfNeeded()) {
        return QStringLiteral("simulation");
    }

    if (!ensureTransportOpen()) {
        return QString();
    }

    QByteArray response;
    if (!queryFlexible(normalized.toLatin1(), &response, 120)) {
        return QString();
    }

    m_state.lastError.clear();
    return QString::fromLatin1(response);
}

void OnStepMountCore::disconnect()
{
    if (m_transport) {
        m_transport->close();
    }

    m_rxBuffer.clear();
    m_state = MountState();
    m_state.totalSlewRates = 10;
    m_state.slewRate = 5;
}

bool OnStepMountCore::isConnected() const
{
    return m_state.connected;
}

MountState OnStepMountCore::state() const
{
    return m_state;
}

bool OnStepMountCore::refreshState()
{
    if (simulateIfNeeded()) {
        if (m_state.slewing) {
            m_state.raHours = m_state.targetRaHours;
            m_state.decDegrees = m_state.targetDecDegrees;
            m_state.slewing = false;
        }
        clampCoordinates();
        return true;
    }

    return refreshCoordinates() && refreshStatus();
}

bool OnStepMountCore::handshake()
{
    if (simulateIfNeeded()) {
        return true;
    }

    QByteArray response;
    if (queryFlexible(OnStepMountProtocol::buildHandshake(), &response) &&
        normalizeHandshake(response).contains(QStringLiteral("onstep"))) {
        return true;
    }

    if (queryFlexible(OnStepMountProtocol::buildHandshake(), &response) &&
        normalizeHandshake(response).contains(QStringLiteral("onstep"))) {
        return true;
    }

    setError(QStringLiteral("OnStep handshake failed"));
    return false;
}

bool OnStepMountCore::gotoRaDec(double raHours, double decDegrees)
{
    if (simulateIfNeeded()) {
        m_state.targetRaHours = raHours;
        m_state.targetDecDegrees = decDegrees;
        m_state.slewing = true;
        return refreshState();
    }

    QByteArray response;
    if (!queryFlexible(OnStepMountProtocol::buildSetTargetRa(raHours), &response) ||
        !OnStepMountProtocol::parseSingleCharSuccess(response)) {
        setError(QStringLiteral("Failed to set OnStep target RA"));
        return false;
    }

    if (!queryFlexible(OnStepMountProtocol::buildSetTargetDec(decDegrees), &response) ||
        !OnStepMountProtocol::parseSingleCharSuccess(response)) {
        setError(QStringLiteral("Failed to set OnStep target DEC"));
        return false;
    }

    if (!queryFlexible(OnStepMountProtocol::buildGoto(), &response)) {
        return false;
    }

    const int code = OnStepMountProtocol::parseGotoErrorCode(response);
    if (code != 0) {
        setError(gotoErrorMessage(code));
        return false;
    }

    m_state.targetRaHours = raHours;
    m_state.targetDecDegrees = decDegrees;
    m_state.slewing = true;
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::syncRaDec(double raHours, double decDegrees)
{
    if (simulateIfNeeded()) {
        m_state.raHours = raHours;
        m_state.decDegrees = decDegrees;
        m_state.targetRaHours = raHours;
        m_state.targetDecDegrees = decDegrees;
        clampCoordinates();
        return true;
    }

    QByteArray response;
    if (!queryFlexible(OnStepMountProtocol::buildSetTargetRa(raHours), &response) ||
        !OnStepMountProtocol::parseSingleCharSuccess(response)) {
        setError(QStringLiteral("Failed to set OnStep sync RA"));
        return false;
    }

    if (!queryFlexible(OnStepMountProtocol::buildSetTargetDec(decDegrees), &response) ||
        !OnStepMountProtocol::parseSingleCharSuccess(response)) {
        setError(QStringLiteral("Failed to set OnStep sync DEC"));
        return false;
    }

    if (!queryFlexible(OnStepMountProtocol::buildSync(), &response) ||
        !OnStepMountProtocol::parseSyncSuccess(response)) {
        setError(QStringLiteral("OnStep sync failed"));
        return false;
    }

    m_state.raHours = raHours;
    m_state.decDegrees = decDegrees;
    m_state.targetRaHours = raHours;
    m_state.targetDecDegrees = decDegrees;
    clampCoordinates();
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::park()
{
    if (simulateIfNeeded()) {
        m_state.parked = true;
        m_state.tracking = false;
        m_state.slewing = false;
        return true;
    }

    if (!writeBlind(OnStepMountProtocol::buildPark())) {
        return false;
    }

    m_state.parked = true;
    m_state.tracking = false;
    m_state.slewing = true;
    return true;
}

bool OnStepMountCore::unpark()
{
    if (simulateIfNeeded()) {
        m_state.parked = false;
        return true;
    }

    QByteArray response;
    if (!queryFlexible(OnStepMountProtocol::buildUnpark(), &response) ||
        !OnStepMountProtocol::parseSingleCharSuccess(response)) {
        setError(QStringLiteral("OnStep unpark failed"));
        return false;
    }

    m_state.parked = false;
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::abort()
{
    if (simulateIfNeeded()) {
        m_state.slewing = false;
        m_state.targetRaHours = m_state.raHours;
        m_state.targetDecDegrees = m_state.decDegrees;
        return true;
    }

    if (!writeBlind(OnStepMountProtocol::buildAbort())) {
        return false;
    }

    refreshState();
    m_state.slewing = false;
    m_state.targetRaHours = m_state.raHours;
    m_state.targetDecDegrees = m_state.decDegrees;
    return true;
}

bool OnStepMountCore::setTrackingEnabled(bool enabled)
{
    if (simulateIfNeeded()) {
        if (m_state.parked && enabled) {
            setError(QStringLiteral("Cannot enable tracking while parked"));
            return false;
        }
        m_state.tracking = enabled;
        return true;
    }

    QByteArray response;
    if (!queryFlexible(OnStepMountProtocol::buildSetTracking(enabled), &response) ||
        !OnStepMountProtocol::parseSingleCharSuccess(response)) {
        setError(enabled ? QStringLiteral("Failed to enable OnStep tracking")
                         : QStringLiteral("Failed to disable OnStep tracking"));
        return false;
    }

    m_state.tracking = enabled;
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::moveNorthSouth(bool north, bool start)
{
    if (!start) {
        return abort();
    }

    if (simulateIfNeeded()) {
        m_state.slewing = true;
        m_state.decDegrees += north ? 0.5 : -0.5;
        clampCoordinates();
        return true;
    }

    if (!writeBlind(OnStepMountProtocol::buildMoveNorthSouth(north))) {
        return false;
    }

    m_state.slewing = true;
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::moveEastWest(bool east, bool start)
{
    if (!start) {
        return abort();
    }

    if (simulateIfNeeded()) {
        m_state.slewing = true;
        m_state.raHours += east ? 0.02 : -0.02;
        clampCoordinates();
        return true;
    }

    if (!writeBlind(OnStepMountProtocol::buildMoveEastWest(east))) {
        return false;
    }

    m_state.slewing = true;
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::setSlewRate(int slewRate)
{
    const int bounded = qBound(0, slewRate, 9);
    if (simulateIfNeeded()) {
        m_state.slewRate = bounded;
        return true;
    }

    if (!writeBlind(OnStepMountProtocol::buildSetSlewRate(bounded))) {
        return false;
    }

    m_state.slewRate = bounded;
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::simulateIfNeeded()
{
    if (!m_transport || m_simulationMode) {
        applySimulationDefaults();
        return true;
    }
    return false;
}

bool OnStepMountCore::ensureTransportOpen()
{
    if (!m_transport) {
        applySimulationDefaults();
        return true;
    }

    if (m_transport->isOpen()) {
        return true;
    }

    if (!m_transport->open()) {
        m_simulationMode = false;
        setError(QStringLiteral("Failed to open mount transport"));
        return false;
    }

    m_state.connected = true;
    m_simulationMode = false;
    return true;
}

bool OnStepMountCore::refreshCoordinates()
{
    QByteArray response;
    double raHours = m_state.raHours;
    double decDegrees = m_state.decDegrees;

    if (!queryTerminated(OnStepMountProtocol::buildGetRa(), &response) ||
        !OnStepMountProtocol::parseRa(response, &raHours)) {
        setError(QStringLiteral("Failed to read OnStep RA"));
        return false;
    }

    if (!queryTerminated(OnStepMountProtocol::buildGetDec(), &response) ||
        !OnStepMountProtocol::parseDec(response, &decDegrees)) {
        setError(QStringLiteral("Failed to read OnStep DEC"));
        return false;
    }

    m_state.raHours = raHours;
    m_state.decDegrees = decDegrees;
    clampCoordinates();
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::refreshStatus()
{
    QByteArray response;
    if (!queryFlexible(OnStepMountProtocol::buildGetStatus(), &response)) {
        return false;
    }

    OnStepMountStatus status;
    if (!OnStepMountProtocol::parseStatus(response, &status)) {
        setError(QStringLiteral("Failed to parse OnStep status"));
        return false;
    }

    m_state.parked = status.parked;
    m_state.slewing = status.slewing;
    m_state.tracking = status.tracking;
    m_state.pierSide = status.raw.contains(QLatin1Char('E')) ? QStringLiteral("EAST")
                                                             : QStringLiteral("UNKNOWN");
    m_state.lastError.clear();
    return true;
}

bool OnStepMountCore::queryTerminated(const QByteArray &command, QByteArray *response)
{
    if (!m_transport || !m_transport->isOpen()) {
        setError(QStringLiteral("Mount transport is not open"));
        return false;
    }

    if (!m_transport->write(command)) {
        setError(QStringLiteral("Failed to write mount command"));
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < m_timeoutMs) {
        const QByteArray chunk = m_transport->read(50);
        if (!chunk.isEmpty()) {
            m_rxBuffer.append(chunk);
        }

        const int terminator = m_rxBuffer.indexOf('#');
        if (terminator >= 0) {
            if (response) {
                *response = m_rxBuffer.left(terminator);
            }
            m_rxBuffer.remove(0, terminator + 1);
            m_state.connected = true;
            return true;
        }
    }

    setError(QStringLiteral("Timed out waiting for terminated OnStep response"));
    return false;
}

bool OnStepMountCore::queryFlexible(const QByteArray &command, QByteArray *response, int settleMs)
{
    if (!m_transport || !m_transport->isOpen()) {
        setError(QStringLiteral("Mount transport is not open"));
        return false;
    }

    m_rxBuffer.clear();
    if (!m_transport->write(command)) {
        setError(QStringLiteral("Failed to write mount command"));
        return false;
    }

    QElapsedTimer timer;
    QElapsedTimer lastDataTimer;
    timer.start();
    lastDataTimer.start();

    while (timer.elapsed() < m_timeoutMs) {
        const QByteArray chunk = m_transport->read(50);
        if (!chunk.isEmpty()) {
            m_rxBuffer.append(chunk);
            lastDataTimer.restart();
            const int terminator = m_rxBuffer.indexOf('#');
            if (terminator >= 0) {
                if (response) {
                    *response = m_rxBuffer.left(terminator);
                }
                m_rxBuffer.remove(0, terminator + 1);
                m_state.connected = true;
                return true;
            }
        }

        if (!m_rxBuffer.isEmpty() && lastDataTimer.elapsed() >= settleMs) {
            if (response) {
                *response = m_rxBuffer.trimmed();
            }
            m_rxBuffer.clear();
            m_state.connected = true;
            return true;
        }
    }

    if (!m_rxBuffer.isEmpty()) {
        if (response) {
            *response = m_rxBuffer.trimmed();
        }
        m_rxBuffer.clear();
        m_state.connected = true;
        return true;
    }

    setError(QStringLiteral("Timed out waiting for OnStep response"));
    return false;
}

bool OnStepMountCore::writeBlind(const QByteArray &command)
{
    if (!m_transport || !m_transport->isOpen()) {
        setError(QStringLiteral("Mount transport is not open"));
        return false;
    }

    if (!m_transport->write(command)) {
        setError(QStringLiteral("Failed to write mount command"));
        return false;
    }

    m_state.connected = true;
    m_state.lastError.clear();
    return true;
}

void OnStepMountCore::applySimulationDefaults()
{
    m_simulationMode = true;
    m_state.connected = true;
    if (m_state.raHours == 0.0 && m_state.decDegrees == 0.0) {
        m_state.raHours = 5.5;
        m_state.decDegrees = 22.0;
        m_state.targetRaHours = m_state.raHours;
        m_state.targetDecDegrees = m_state.decDegrees;
    }
    m_state.tracking = true;
    m_state.pierSide = QStringLiteral("EAST");
    m_state.lastError.clear();
}

void OnStepMountCore::clampCoordinates()
{
    while (m_state.raHours < 0.0) {
        m_state.raHours += 24.0;
    }
    while (m_state.raHours >= 24.0) {
        m_state.raHours -= 24.0;
    }
    m_state.decDegrees = qBound(-90.0, m_state.decDegrees, 90.0);
}

void OnStepMountCore::setError(const QString &message)
{
    m_state.connected = false;
    m_state.lastError = message;
}

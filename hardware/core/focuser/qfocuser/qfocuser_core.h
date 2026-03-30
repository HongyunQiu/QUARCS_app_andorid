#ifndef QFOCUSER_CORE_H
#define QFOCUSER_CORE_H

#include "hardware/core/focuser/qfocuser/qfocuser_protocol.h"
#include "hardware/devices/focuser_device_interface.h"

#include <memory>

class ITransport;

class QFocuserCore
{
public:
    explicit QFocuserCore(std::unique_ptr<ITransport> transport, int timeoutMs = 1000);

    bool connect();
    void disconnect();
    bool isConnected() const;

    FocuserState state() const;
    bool refreshState();
    bool handshake();
    bool refreshPosition();
    bool moveAbsolute(int position);
    bool moveRelative(bool outward, int steps);
    bool abort();
    bool syncPosition(int position);
    bool setSpeed(int speed);
    bool setReverse(bool enabled);

private:
    bool simulateIfNeeded();
    bool transact(const QByteArray &command, int expectedIdx, QJsonObject *response = nullptr);
    void applySimulationDefaults();
    void setError(const QString &message);

    std::unique_ptr<ITransport> m_transport;
    int m_timeoutMs = 1000;
    bool m_simulationMode = false;
    QByteArray m_rxBuffer;
    FocuserState m_state;
    QFocuserVersionInfo m_version;
    QFocuserTelemetry m_telemetry;
};

#endif // QFOCUSER_CORE_H

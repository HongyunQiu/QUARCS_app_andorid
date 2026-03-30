#include "hardware/transports/tcp_transport.h"

#include <QTcpSocket>

TcpTransport::TcpTransport(const QString &host, quint16 port, int timeoutMs)
    : m_host(host)
    , m_port(port)
    , m_timeoutMs(timeoutMs)
{
}

TcpTransport::~TcpTransport()
{
    close();
}

bool TcpTransport::open()
{
    if (isOpen()) {
        return true;
    }

    if (!m_socket) {
        m_socket = new QTcpSocket();
    }

    m_socket->connectToHost(m_host, m_port);
    return m_socket->waitForConnected(m_timeoutMs);
}

void TcpTransport::close()
{
    if (!m_socket) {
        return;
    }

    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected(200);
    }

    delete m_socket;
    m_socket = nullptr;
}

bool TcpTransport::isOpen() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

bool TcpTransport::write(const QByteArray &payload)
{
    if (!isOpen() && !const_cast<TcpTransport *>(this)->open()) {
        return false;
    }

    if (m_socket->write(payload) < 0) {
        return false;
    }

    return m_socket->waitForBytesWritten(m_timeoutMs);
}

QByteArray TcpTransport::read(int timeoutMs)
{
    if (!isOpen()) {
        return {};
    }

    if (m_socket->bytesAvailable() == 0 && !m_socket->waitForReadyRead(timeoutMs > 0 ? timeoutMs : m_timeoutMs)) {
        return {};
    }

    return m_socket->readAll();
}

#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H

#include "hardware/transports/transport_interface.h"

#include <QString>

class QTcpSocket;

class TcpTransport : public ITransport
{
public:
    TcpTransport(const QString &host, quint16 port, int timeoutMs = 3000);
    ~TcpTransport() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(const QByteArray &payload) override;
    QByteArray read(int timeoutMs) override;

private:
    QString m_host;
    quint16 m_port = 0;
    int m_timeoutMs = 3000;
    QTcpSocket *m_socket = nullptr;
};

#endif // TCP_TRANSPORT_H

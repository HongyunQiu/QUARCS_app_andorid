#ifndef TRANSPORT_INTERFACE_H
#define TRANSPORT_INTERFACE_H

#include <QByteArray>

class ITransport
{
public:
    virtual ~ITransport() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool write(const QByteArray &payload) = 0;
    virtual QByteArray read(int timeoutMs) = 0;
};

#endif // TRANSPORT_INTERFACE_H

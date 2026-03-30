#ifndef USB_SERIAL_TRANSPORT_H
#define USB_SERIAL_TRANSPORT_H

#include "hardware/transports/transport_interface.h"

#include <QString>

class UsbSerialTransport : public ITransport
{
public:
    UsbSerialTransport(const QString &devicePath, int baudRate = 9600);
    ~UsbSerialTransport() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(const QByteArray &payload) override;
    QByteArray read(int timeoutMs) override;

private:
    bool configurePort();

    QString m_devicePath;
    int m_baudRate = 9600;
    int m_fd = -1;
};

#endif // USB_SERIAL_TRANSPORT_H

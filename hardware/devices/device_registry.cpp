#include "hardware/devices/device_registry.h"

#include "hardware/adapters/camera/default_camera_device.h"
#include "hardware/adapters/camera/qhyccd_camera_adapter.h"
#include "hardware/adapters/focuser/qfocuser_adapter.h"
#include "hardware/adapters/mount/onstep_mount_adapter.h"
#include "hardware/transports/tcp_transport.h"
#include "hardware/transports/transport_interface.h"
#include "hardware/transports/usb_serial_transport.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QSettings>
#include <memory>

namespace {

std::unique_ptr<ICameraDevice> &cameraDeviceStorage()
{
    static std::unique_ptr<ICameraDevice> device;
    return device;
}

std::unique_ptr<ICameraDevice> createCameraDevice()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QSettings settings(QStringLiteral("QUARCS"), QStringLiteral("EmbeddedFrontend"));

    const QString configuredBackend = settings.value(QStringLiteral("bridge/MainCameraBackend")).toString().trimmed();
    const QString backend = !configuredBackend.isEmpty()
        ? configuredBackend.toLower()
        : env.value(QStringLiteral("QUARCS_CAMERA_BACKEND"), QStringLiteral("qhyccd")).trimmed().toLower();

    if (backend == QStringLiteral("default") || backend == QStringLiteral("simulation")) {
        qDebug() << "DeviceRegistry selected default camera backend";
        return std::unique_ptr<ICameraDevice>(new DefaultCameraDevice());
    }

    qDebug() << "DeviceRegistry selected QHYCCD camera backend";
    return std::unique_ptr<ICameraDevice>(new QhyccdCameraAdapter());
}

std::unique_ptr<IMountDevice> &mountDeviceStorage()
{
    static std::unique_ptr<IMountDevice> device;
    return device;
}

std::unique_ptr<IFocuserDevice> &focuserDeviceStorage()
{
    static std::unique_ptr<IFocuserDevice> device;
    return device;
}

QString detectDefaultSerialPort()
{
    const QDir devDir(QStringLiteral("/dev"));
    const QStringList candidates = devDir.entryList(
        QStringList()
            << QStringLiteral("ttyACM*")
            << QStringLiteral("ttyUSB*")
            << QStringLiteral("ttyS*"),
        QDir::System | QDir::Files,
        QDir::Name);

    if (candidates.isEmpty()) {
        return {};
    }

    return devDir.absoluteFilePath(candidates.constFirst());
}

std::unique_ptr<ITransport> createFocuserTransport()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QSettings settings(QStringLiteral("QUARCS"), QStringLiteral("EmbeddedFrontend"));

    const QString configuredTcpEndpoint = settings.value(QStringLiteral("bridge/FocuserTcpEndpoint")).toString().trimmed();
    const QString configuredSerialPort = settings.value(QStringLiteral("bridge/FocuserSerialPort")).toString().trimmed();
    const QString configuredBaud = settings.value(QStringLiteral("bridge/FocuserBaudRate"), QStringLiteral("9600")).toString().trimmed();

    const QString tcpEndpoint = !configuredTcpEndpoint.isEmpty()
        ? configuredTcpEndpoint
        : env.value(QStringLiteral("QUARCS_FOCUSER_TCP"));

    if (!tcpEndpoint.isEmpty()) {
        const int separator = tcpEndpoint.lastIndexOf(QLatin1Char(':'));
        if (separator > 0) {
            const QString host = tcpEndpoint.left(separator);
            bool ok = false;
            const int port = tcpEndpoint.mid(separator + 1).toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                qDebug() << "DeviceRegistry selected focuser TCP transport" << tcpEndpoint;
                return std::unique_ptr<ITransport>(new TcpTransport(host, static_cast<quint16>(port)));
            }
        }
    }

    const QString serialPort = !configuredSerialPort.isEmpty()
        ? configuredSerialPort
        : (!env.value(QStringLiteral("QUARCS_FOCUSER_SERIAL_PORT")).isEmpty()
              ? env.value(QStringLiteral("QUARCS_FOCUSER_SERIAL_PORT"))
              : detectDefaultSerialPort());
    if (!serialPort.isEmpty()) {
        bool ok = false;
        const QString baudText = !configuredBaud.isEmpty()
            ? configuredBaud
            : env.value(QStringLiteral("QUARCS_FOCUSER_BAUD"), QStringLiteral("9600"));
        const int baudRate = baudText.toInt(&ok);
        qDebug() << "DeviceRegistry selected focuser serial transport" << serialPort << "baud" << (ok ? baudRate : 9600);
        return std::unique_ptr<ITransport>(new UsbSerialTransport(serialPort, ok ? baudRate : 9600));
    }

    qWarning() << "DeviceRegistry did not find focuser transport, simulation mode will be used";
    return std::unique_ptr<ITransport>();
}

std::unique_ptr<ITransport> createMountTransport()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QSettings settings(QStringLiteral("QUARCS"), QStringLiteral("EmbeddedFrontend"));

    const QString configuredTcpEndpoint = settings.value(QStringLiteral("bridge/MountTcpEndpoint")).toString().trimmed();
    const QString configuredSerialPort = settings.value(QStringLiteral("bridge/MountSerialPort")).toString().trimmed();
    const QString configuredBaud = settings.value(QStringLiteral("bridge/MountBaudRate"), QStringLiteral("9600")).toString().trimmed();

    const QString tcpEndpoint = !configuredTcpEndpoint.isEmpty()
        ? configuredTcpEndpoint
        : env.value(QStringLiteral("QUARCS_MOUNT_TCP"));

    if (!tcpEndpoint.isEmpty()) {
        const int separator = tcpEndpoint.lastIndexOf(QLatin1Char(':'));
        if (separator > 0) {
            const QString host = tcpEndpoint.left(separator);
            bool ok = false;
            const int port = tcpEndpoint.mid(separator + 1).toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                qDebug() << "DeviceRegistry selected mount TCP transport" << tcpEndpoint;
                return std::unique_ptr<ITransport>(new TcpTransport(host, static_cast<quint16>(port)));
            }
        }
    }

    const QString serialPort = !configuredSerialPort.isEmpty()
        ? configuredSerialPort
        : env.value(QStringLiteral("QUARCS_MOUNT_SERIAL_PORT"));
    if (!serialPort.isEmpty()) {
        bool ok = false;
        const QString baudText = !configuredBaud.isEmpty()
            ? configuredBaud
            : env.value(QStringLiteral("QUARCS_MOUNT_BAUD"), QStringLiteral("9600"));
        const int baudRate = baudText.toInt(&ok);
        qDebug() << "DeviceRegistry selected mount serial transport" << serialPort << "baud" << (ok ? baudRate : 9600);
        return std::unique_ptr<ITransport>(new UsbSerialTransport(serialPort, ok ? baudRate : 9600));
    }

    qWarning() << "DeviceRegistry did not find mount transport, simulation mode will be used";
    return std::unique_ptr<ITransport>();
}

}

ICameraDevice &DeviceRegistry::cameraDevice()
{
    std::unique_ptr<ICameraDevice> &device = cameraDeviceStorage();
    if (!device) {
        device = createCameraDevice();
    }
    return *device;
}

IMountDevice &DeviceRegistry::mountDevice()
{
    std::unique_ptr<IMountDevice> &device = mountDeviceStorage();
    if (!device) {
        device.reset(new OnStepMountAdapter(createMountTransport()));
    }
    return *device;
}

IFocuserDevice &DeviceRegistry::focuserDevice()
{
    std::unique_ptr<IFocuserDevice> &device = focuserDeviceStorage();
    if (!device) {
        device.reset(new QFocuserAdapter(createFocuserTransport()));
    }
    return *device;
}

void DeviceRegistry::reloadCameraDevice()
{
    std::unique_ptr<ICameraDevice> &device = cameraDeviceStorage();
    if (device) {
        device->disconnect();
    }
    device = createCameraDevice();
}

void DeviceRegistry::reloadFocuserDevice()
{
    std::unique_ptr<IFocuserDevice> &device = focuserDeviceStorage();
    if (device) {
        device->disconnect();
    }
    device.reset(new QFocuserAdapter(createFocuserTransport()));
}

void DeviceRegistry::reloadMountDevice()
{
    std::unique_ptr<IMountDevice> &device = mountDeviceStorage();
    if (device) {
        device->disconnect();
    }
    device.reset(new OnStepMountAdapter(createMountTransport()));
}

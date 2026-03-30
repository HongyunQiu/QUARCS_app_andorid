#include "hardware/transports/usb_serial_transport.h"

#include <QByteArray>
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

#ifdef Q_OS_UNIX
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

namespace {

#ifdef Q_OS_ANDROID
const char *kUsbSerialBridgeClass = "org/qtproject/example/QUARCS_app/UsbSerialBridge";

QString usbBridgeLastError()
{
    const QJniObject error = QJniObject::callStaticObjectMethod(
        kUsbSerialBridgeClass,
        "getLastError",
        "()Ljava/lang/String;");
    return error.toString();
}
#endif

#ifdef Q_OS_UNIX
speed_t toTermiosBaud(int baudRate)
{
    switch (baudRate) {
    case 115200:
        return B115200;
    case 57600:
        return B57600;
    case 38400:
        return B38400;
    case 19200:
        return B19200;
    case 9600:
    default:
        return B9600;
    }
}
#endif

}

UsbSerialTransport::UsbSerialTransport(const QString &devicePath, int baudRate)
    : m_devicePath(devicePath)
    , m_baudRate(baudRate)
{
}

UsbSerialTransport::~UsbSerialTransport()
{
    close();
}

bool UsbSerialTransport::open()
{
#ifdef Q_OS_ANDROID
    if (isOpen()) {
        return true;
    }

    const jboolean ok = QJniObject::callStaticMethod<jboolean>(
        kUsbSerialBridgeClass,
        "open",
        "(Ljava/lang/String;I)Z",
        QJniObject::fromString(m_devicePath).object<jstring>(),
        jint(m_baudRate));
    if (!ok) {
        qWarning() << "UsbSerialTransport open failed" << m_devicePath << "baud" << m_baudRate << usbBridgeLastError();
        return false;
    }

    qDebug() << "UsbSerialTransport opened via Android USB bridge" << m_devicePath << "baud" << m_baudRate;
    return true;
#elif defined(Q_OS_UNIX)
    if (isOpen()) {
        return true;
    }

    m_fd = ::open(m_devicePath.toUtf8().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        qWarning() << "UsbSerialTransport open failed" << m_devicePath << "baud" << m_baudRate << "errno" << errno;
        return false;
    }

    if (!configurePort()) {
        qWarning() << "UsbSerialTransport configure failed" << m_devicePath << "baud" << m_baudRate << "errno" << errno;
        close();
        return false;
    }

    qDebug() << "UsbSerialTransport opened" << m_devicePath << "baud" << m_baudRate;
    return true;
#else
    return false;
#endif
}

void UsbSerialTransport::close()
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(kUsbSerialBridgeClass, "close", "()V");
#elif defined(Q_OS_UNIX)
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

bool UsbSerialTransport::isOpen() const
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>(kUsbSerialBridgeClass, "isOpen", "()Z");
#else
    return m_fd >= 0;
#endif
}

bool UsbSerialTransport::write(const QByteArray &payload)
{
#ifdef Q_OS_ANDROID
    if (!isOpen() && !const_cast<UsbSerialTransport *>(this)->open()) {
        return false;
    }

    QJniEnvironment env;
    jbyteArray jPayload = env->NewByteArray(payload.size());
    env->SetByteArrayRegion(jPayload, 0, payload.size(), reinterpret_cast<const jbyte *>(payload.constData()));
    const jboolean ok = QJniObject::callStaticMethod<jboolean>(
        kUsbSerialBridgeClass,
        "write",
        "([BI)Z",
        jPayload,
        jint(2000));
    env->DeleteLocalRef(jPayload);
    if (!ok) {
        qWarning() << "UsbSerialTransport write failed" << usbBridgeLastError();
    }
    return ok;
#elif defined(Q_OS_UNIX)
    if (!isOpen() && !const_cast<UsbSerialTransport *>(this)->open()) {
        return false;
    }

    const char *data = payload.constData();
    qint64 remaining = payload.size();
    while (remaining > 0) {
        const ssize_t written = ::write(m_fd, data, static_cast<size_t>(remaining));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        remaining -= written;
        data += written;
    }

    return true;
#else
    Q_UNUSED(payload)
    return false;
#endif
}

QByteArray UsbSerialTransport::read(int timeoutMs)
{
#ifdef Q_OS_ANDROID
    if (!isOpen()) {
        return {};
    }

    const QJniObject result = QJniObject::callStaticObjectMethod(
        kUsbSerialBridgeClass,
        "read",
        "(I)[B",
        jint(timeoutMs));
    if (!result.isValid()) {
        return {};
    }

    QJniEnvironment env;
    jbyteArray byteArray = result.object<jbyteArray>();
    if (!byteArray) {
        return {};
    }

    const jsize length = env->GetArrayLength(byteArray);
    if (length <= 0) {
        return {};
    }

    QByteArray buffer(length, Qt::Uninitialized);
    env->GetByteArrayRegion(byteArray, 0, length, reinterpret_cast<jbyte *>(buffer.data()));
    return buffer;
#elif defined(Q_OS_UNIX)
    if (!isOpen()) {
        return {};
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_fd, &readSet);

    timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    const int ready = ::select(m_fd + 1, &readSet, nullptr, nullptr, &timeout);
    if (ready <= 0 || !FD_ISSET(m_fd, &readSet)) {
        return {};
    }

    QByteArray buffer(512, Qt::Uninitialized);
    const ssize_t bytesRead = ::read(m_fd, buffer.data(), static_cast<size_t>(buffer.size()));
    if (bytesRead <= 0) {
        return {};
    }

    buffer.truncate(static_cast<int>(bytesRead));
    return buffer;
#else
    Q_UNUSED(timeoutMs)
    return {};
#endif
}

bool UsbSerialTransport::configurePort()
{
#ifdef Q_OS_ANDROID
    return true;
#elif defined(Q_OS_UNIX)
    termios options;
    if (::tcgetattr(m_fd, &options) != 0) {
        return false;
    }

    ::cfmakeraw(&options);
    ::cfsetispeed(&options, toTermiosBaud(m_baudRate));
    ::cfsetospeed(&options, toTermiosBaud(m_baudRate));
    options.c_cflag |= CLOCAL | CREAD;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    return ::tcsetattr(m_fd, TCSANOW, &options) == 0;
#else
    return false;
#endif
}

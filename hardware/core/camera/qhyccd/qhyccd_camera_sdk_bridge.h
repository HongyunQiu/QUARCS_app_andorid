#ifndef QHYCCD_CAMERA_SDK_BRIDGE_H
#define QHYCCD_CAMERA_SDK_BRIDGE_H

#include "hardware/devices/camera_device_interface.h"

#include <QByteArray>
#include <QString>

class QhyccdCameraSdkBridge
{
public:
    QhyccdCameraSdkBridge();
    ~QhyccdCameraSdkBridge();

    bool open();
    void close();
    bool isOpen() const;

    CameraState state() const;
    CameraCapabilities capabilities() const;
    bool refreshState();
    bool startExposure(double seconds, bool lightFrame);
    bool abortExposure();
    bool readoutFrame(QByteArray *buffer);
    bool setGain(int gain);
    bool setOffset(int offset);
    bool setFrameSpec(const CameraFrameSpec &spec);
    bool setFrameType(CameraFrameType frameType);
    bool setCoolerEnabled(bool enabled);
    bool setTargetTemperature(double celsius);

private:
    bool openFirstCamera();
    bool applyInitialConfiguration();
    void refreshCapabilities();
    bool syncStateFromCamera();
    bool ensureFrameBuffer();
    bool prepareUsbAccess(int timeoutMs);
    QString describeUsbState() const;
    bool primeSdkWithAndroidUsbHandles();
    bool setLastError(const QString &message);
    CameraFrameSpec sanitizeFrameSpec(const CameraFrameSpec &spec) const;

    CameraState m_state;
    CameraCapabilities m_capabilities;
    bool m_lastFrameWasLight = true;
    bool m_resourceInitialized = false;
    void *m_handle = nullptr;
    QByteArray m_cameraId;
    QByteArray m_cameraModel;
    QByteArray m_frameBuffer;
};

#endif // QHYCCD_CAMERA_SDK_BRIDGE_H

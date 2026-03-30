#ifndef QHYCCD_CAMERA_CORE_H
#define QHYCCD_CAMERA_CORE_H

#include "hardware/devices/camera_device_interface.h"

#include <memory>

class QhyccdCameraSdkBridge;

class QhyccdCameraCore
{
public:
    explicit QhyccdCameraCore(std::unique_ptr<QhyccdCameraSdkBridge> sdkBridge);

    bool connect();
    void disconnect();
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
    void setError(const QString &message);

    std::unique_ptr<QhyccdCameraSdkBridge> m_sdkBridge;
    CameraState m_state;
    CameraCapabilities m_capabilities;
};

#endif // QHYCCD_CAMERA_CORE_H

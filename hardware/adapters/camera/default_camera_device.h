#ifndef DEFAULT_CAMERA_DEVICE_H
#define DEFAULT_CAMERA_DEVICE_H

#include "hardware/devices/camera_device_interface.h"

class DefaultCameraDevice : public ICameraDevice
{
public:
    bool connect() override;
    void disconnect() override;
    CameraState state() const override;
    CameraCapabilities capabilities() const override;
    bool refreshState() override;
    bool startExposure(double seconds, bool lightFrame) override;
    bool abortExposure() override;
    bool readoutFrame(QByteArray *buffer) override;
    bool setGain(int gain) override;
    bool setOffset(int offset) override;
    bool setFrameSpec(const CameraFrameSpec &spec) override;
    bool setFrameType(CameraFrameType frameType) override;
    bool setCoolerEnabled(bool enabled) override;
    bool setTargetTemperature(double celsius) override;

private:
    void ensureConnected();
    CameraFrameSpec sanitizedFrameSpec(const CameraFrameSpec &spec) const;

    CameraState m_state;
    CameraCapabilities m_capabilities;
    bool m_lastFrameWasLight = true;
};

#endif // DEFAULT_CAMERA_DEVICE_H

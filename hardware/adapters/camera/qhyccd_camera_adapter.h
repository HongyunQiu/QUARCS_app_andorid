#ifndef QHYCCD_CAMERA_ADAPTER_H
#define QHYCCD_CAMERA_ADAPTER_H

#include "hardware/devices/camera_device_interface.h"

#include <memory>

class QhyccdCameraCore;

class QhyccdCameraAdapter : public ICameraDevice
{
public:
    QhyccdCameraAdapter();
    ~QhyccdCameraAdapter() override;

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
    std::unique_ptr<QhyccdCameraCore> m_core;
};

#endif // QHYCCD_CAMERA_ADAPTER_H

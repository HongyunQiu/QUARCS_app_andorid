#include "hardware/adapters/camera/qhyccd_camera_adapter.h"

#include "hardware/core/camera/qhyccd/qhyccd_camera_core.h"
#include "hardware/core/camera/qhyccd/qhyccd_camera_sdk_bridge.h"

QhyccdCameraAdapter::QhyccdCameraAdapter()
    : m_core(new QhyccdCameraCore(std::unique_ptr<QhyccdCameraSdkBridge>(new QhyccdCameraSdkBridge())))
{
}

QhyccdCameraAdapter::~QhyccdCameraAdapter() = default;

bool QhyccdCameraAdapter::connect()
{
    return m_core->connect();
}

void QhyccdCameraAdapter::disconnect()
{
    m_core->disconnect();
}

CameraState QhyccdCameraAdapter::state() const
{
    return m_core->state();
}

CameraCapabilities QhyccdCameraAdapter::capabilities() const
{
    return m_core->capabilities();
}

bool QhyccdCameraAdapter::refreshState()
{
    return m_core->refreshState();
}

bool QhyccdCameraAdapter::startExposure(double seconds, bool lightFrame)
{
    return m_core->startExposure(seconds, lightFrame);
}

bool QhyccdCameraAdapter::abortExposure()
{
    return m_core->abortExposure();
}

bool QhyccdCameraAdapter::readoutFrame(QByteArray *buffer)
{
    return m_core->readoutFrame(buffer);
}

bool QhyccdCameraAdapter::setGain(int gain)
{
    return m_core->setGain(gain);
}

bool QhyccdCameraAdapter::setOffset(int offset)
{
    return m_core->setOffset(offset);
}

bool QhyccdCameraAdapter::setFrameSpec(const CameraFrameSpec &spec)
{
    return m_core->setFrameSpec(spec);
}

bool QhyccdCameraAdapter::setFrameType(CameraFrameType frameType)
{
    return m_core->setFrameType(frameType);
}

bool QhyccdCameraAdapter::setCoolerEnabled(bool enabled)
{
    return m_core->setCoolerEnabled(enabled);
}

bool QhyccdCameraAdapter::setTargetTemperature(double celsius)
{
    return m_core->setTargetTemperature(celsius);
}

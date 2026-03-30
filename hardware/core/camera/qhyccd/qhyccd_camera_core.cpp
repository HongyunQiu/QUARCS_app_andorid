#include "hardware/core/camera/qhyccd/qhyccd_camera_core.h"

#include "hardware/core/camera/qhyccd/qhyccd_camera_sdk_bridge.h"

QhyccdCameraCore::QhyccdCameraCore(std::unique_ptr<QhyccdCameraSdkBridge> sdkBridge)
    : m_sdkBridge(std::move(sdkBridge))
{
    m_state.cameraName = QStringLiteral("QHYCCD");
}

bool QhyccdCameraCore::connect()
{
    if (!m_sdkBridge) {
        setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        return false;
    }

    if (!m_sdkBridge->open()) {
        m_state = m_sdkBridge->state();
        if (m_state.lastError.isEmpty()) {
            setError(QStringLiteral("Failed to open QHYCCD camera"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

void QhyccdCameraCore::disconnect()
{
    if (m_sdkBridge) {
        m_sdkBridge->close();
        m_state = m_sdkBridge->state();
        m_capabilities = m_sdkBridge->capabilities();
    } else {
        m_state = CameraState();
        m_capabilities = CameraCapabilities();
    }
}

CameraState QhyccdCameraCore::state() const
{
    return m_state;
}

CameraCapabilities QhyccdCameraCore::capabilities() const
{
    return m_capabilities;
}

bool QhyccdCameraCore::refreshState()
{
    if (!m_sdkBridge) {
        setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        return false;
    }

    if (!m_sdkBridge->refreshState()) {
        m_state = m_sdkBridge->state();
        m_capabilities = m_sdkBridge->capabilities();
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::startExposure(double seconds, bool lightFrame)
{
    if (!m_sdkBridge || !m_sdkBridge->startExposure(seconds, lightFrame)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::abortExposure()
{
    if (!m_sdkBridge || !m_sdkBridge->abortExposure()) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::readoutFrame(QByteArray *buffer)
{
    if (!m_sdkBridge || !m_sdkBridge->readoutFrame(buffer)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::setGain(int gain)
{
    if (!m_sdkBridge || !m_sdkBridge->setGain(gain)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::setOffset(int offset)
{
    if (!m_sdkBridge || !m_sdkBridge->setOffset(offset)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::setFrameSpec(const CameraFrameSpec &spec)
{
    if (!m_sdkBridge || !m_sdkBridge->setFrameSpec(spec)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::setFrameType(CameraFrameType frameType)
{
    if (!m_sdkBridge || !m_sdkBridge->setFrameType(frameType)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::setCoolerEnabled(bool enabled)
{
    if (!m_sdkBridge || !m_sdkBridge->setCoolerEnabled(enabled)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

bool QhyccdCameraCore::setTargetTemperature(double celsius)
{
    if (!m_sdkBridge || !m_sdkBridge->setTargetTemperature(celsius)) {
        if (m_sdkBridge) {
            m_state = m_sdkBridge->state();
            m_capabilities = m_sdkBridge->capabilities();
        } else {
            setError(QStringLiteral("QHYCCD SDK bridge is unavailable"));
        }
        return false;
    }

    m_state = m_sdkBridge->state();
    m_capabilities = m_sdkBridge->capabilities();
    return true;
}

void QhyccdCameraCore::setError(const QString &message)
{
    m_state.connected = false;
    m_state.lastError = message;
    m_state.exposureState = CameraExposureState::Error;
    m_capabilities = CameraCapabilities();
}

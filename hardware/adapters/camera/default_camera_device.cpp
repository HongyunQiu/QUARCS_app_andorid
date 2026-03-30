#include "hardware/adapters/camera/default_camera_device.h"

#include <QtGlobal>

bool DefaultCameraDevice::connect()
{
    ensureConnected();
    return true;
}

void DefaultCameraDevice::disconnect()
{
    m_state = CameraState();
    m_capabilities = CameraCapabilities();
    m_lastFrameWasLight = true;
}

CameraState DefaultCameraDevice::state() const
{
    return m_state;
}

CameraCapabilities DefaultCameraDevice::capabilities() const
{
    return m_capabilities;
}

bool DefaultCameraDevice::refreshState()
{
    ensureConnected();
    if (m_state.exposing) {
        m_state.exposing = false;
        m_state.frameReady = true;
        m_state.exposureState = CameraExposureState::ReadingOut;
    } else if (m_state.exposureState == CameraExposureState::ReadingOut) {
        m_state.exposureState = CameraExposureState::Idle;
    }
    return true;
}

bool DefaultCameraDevice::startExposure(double seconds, bool lightFrame)
{
    ensureConnected();
    if (seconds <= 0.0) {
        m_state.lastError = QStringLiteral("Exposure seconds must be positive");
        m_state.exposureState = CameraExposureState::Error;
        return false;
    }

    m_state.exposing = true;
    m_state.frameReady = false;
    m_state.exposureSeconds = seconds;
    m_state.exposureState = CameraExposureState::Exposing;
    m_lastFrameWasLight = lightFrame;
    m_state.lastError.clear();
    return true;
}

bool DefaultCameraDevice::abortExposure()
{
    ensureConnected();
    m_state.exposing = false;
    m_state.frameReady = false;
    m_state.exposureState = CameraExposureState::Idle;
    m_state.exposureSeconds = 0.0;
    return true;
}

bool DefaultCameraDevice::readoutFrame(QByteArray *buffer)
{
    ensureConnected();
    if (!buffer) {
        m_state.lastError = QStringLiteral("Frame buffer is null");
        return false;
    }

    if (m_state.exposing) {
        m_state.lastError = QStringLiteral("Exposure still in progress");
        return false;
    }

    if (!m_state.frameReady) {
        m_state.lastError = QStringLiteral("No frame available");
        return false;
    }

    m_state.exposureState = CameraExposureState::Downloading;
    const QString frameSummary = QStringLiteral("SIM_FRAME:%1:%2x%3:bin=%4x%5:gain=%6:offset=%7")
                                     .arg(m_lastFrameWasLight ? QStringLiteral("LIGHT") : QStringLiteral("DARK"))
                                     .arg(m_state.frameSpec.width)
                                     .arg(m_state.frameSpec.height)
                                     .arg(m_state.frameSpec.binX)
                                     .arg(m_state.frameSpec.binY)
                                     .arg(m_state.gain)
                                     .arg(m_state.offset);
    *buffer = frameSummary.toUtf8();
    m_state.frameReady = false;
    m_state.exposureState = CameraExposureState::Idle;
    m_state.lastError.clear();
    return true;
}

bool DefaultCameraDevice::setGain(int gain)
{
    ensureConnected();
    m_state.gain = qMax(0, gain);
    return true;
}

bool DefaultCameraDevice::setOffset(int offset)
{
    ensureConnected();
    m_state.offset = qMax(0, offset);
    return true;
}

bool DefaultCameraDevice::setFrameSpec(const CameraFrameSpec &spec)
{
    ensureConnected();
    m_state.frameSpec = sanitizedFrameSpec(spec);
    return true;
}

bool DefaultCameraDevice::setFrameType(CameraFrameType frameType)
{
    ensureConnected();
    m_state.frameType = frameType;
    return true;
}

bool DefaultCameraDevice::setCoolerEnabled(bool enabled)
{
    ensureConnected();
    if (!m_state.coolerSupported && enabled) {
        m_state.lastError = QStringLiteral("Cooler is not supported");
        return false;
    }

    m_state.coolerEnabled = enabled;
    return true;
}

bool DefaultCameraDevice::setTargetTemperature(double celsius)
{
    ensureConnected();
    m_state.targetTemperature = celsius;
    return true;
}

void DefaultCameraDevice::ensureConnected()
{
    if (!m_state.connected) {
        m_state.connected = true;
        m_state.cameraName = QStringLiteral("AndroidCameraSimulator");
        m_state.coolerSupported = true;
        m_state.sensorTemperature = 20.0;
        m_state.targetTemperature = 0.0;
        m_state.frameSpec = sanitizedFrameSpec(m_state.frameSpec);
        m_capabilities.exposureSupported = true;
        m_capabilities.gainSupported = true;
        m_capabilities.offsetSupported = true;
        m_capabilities.coolerSupported = true;
        m_capabilities.targetTemperatureSupported = true;
        m_capabilities.roiSupported = true;
        m_capabilities.binningSupported = true;
        m_capabilities.frameTypeSupported = true;
        m_capabilities.readModesSupported = false;
        m_capabilities.maxBinX = 4;
        m_capabilities.maxBinY = 4;
        m_capabilities.exposureSeconds = {true, 0.001, 3600.0, 0.001, m_state.exposureSeconds, QStringLiteral("s")};
        m_capabilities.gain = {true, 0.0, 100.0, 1.0, static_cast<double>(m_state.gain), QStringLiteral("adu")};
        m_capabilities.offset = {true, 0.0, 255.0, 1.0, static_cast<double>(m_state.offset), QStringLiteral("adu")};
        m_capabilities.targetTemperature = {true, -30.0, 30.0, 0.1, m_state.targetTemperature, QStringLiteral("C")};
        m_capabilities.frameTypes = {QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("Flat"), QStringLiteral("Bias")};
    }

    m_capabilities.coolerSupported = m_state.coolerSupported;
    m_capabilities.targetTemperatureSupported = m_state.coolerSupported;
    m_capabilities.exposureSeconds.current = m_state.exposureSeconds;
    m_capabilities.gain.current = m_state.gain;
    m_capabilities.offset.current = m_state.offset;
    m_capabilities.targetTemperature.current = m_state.targetTemperature;
}

CameraFrameSpec DefaultCameraDevice::sanitizedFrameSpec(const CameraFrameSpec &spec) const
{
    CameraFrameSpec sanitized = spec;
    sanitized.x = qMax(0, sanitized.x);
    sanitized.y = qMax(0, sanitized.y);
    sanitized.width = qMax(1, sanitized.width);
    sanitized.height = qMax(1, sanitized.height);
    sanitized.binX = qMax(1, sanitized.binX);
    sanitized.binY = qMax(1, sanitized.binY);
    return sanitized;
}

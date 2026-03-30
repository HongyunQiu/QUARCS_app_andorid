#include "hardware/core/camera/qhyccd/qhyccd_camera_sdk_bridge.h"

#ifdef Q_OS_ANDROID
#include "qhyccd.h"

#include <QJniObject>
#include <QThread>
#endif

#include <QDebug>
#include <QStringList>
#include <QtGlobal>

namespace {

QStringList defaultFrameTypes()
{
    return {QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("Flat"), QStringLiteral("Bias")};
}

#ifdef Q_OS_ANDROID
CameraControlRange queryRange(qhyccd_handle *handle, CONTROL_ID controlId, double current, double scale, const QString &unit)
{
    CameraControlRange range;
    range.unit = unit;
    range.current = current;

    if (IsQHYCCDControlAvailable(handle, controlId) != QHYCCD_SUCCESS) {
        return range;
    }

    double minimum = 0.0;
    double maximum = 0.0;
    double step = 0.0;
    if (GetQHYCCDParamMinMaxStep(handle, controlId, &minimum, &maximum, &step) != QHYCCD_SUCCESS) {
        return range;
    }

    range.supported = true;
    range.minimum = minimum / scale;
    range.maximum = maximum / scale;
    range.step = step / scale;
    range.current = current;
    return range;
}
#endif

}

QhyccdCameraSdkBridge::QhyccdCameraSdkBridge()
{
    m_state.cameraName = QStringLiteral("QHYCCD-AndroidBridge");
    m_state.coolerSupported = true;
    m_state.bitDepth = 16;
    m_state.gain = 20;
    m_state.offset = 140;
    m_capabilities.frameTypes = defaultFrameTypes();
}

QhyccdCameraSdkBridge::~QhyccdCameraSdkBridge()
{
    close();
}

bool QhyccdCameraSdkBridge::open()
{
#ifdef Q_OS_ANDROID
    if (m_state.connected) {
        return true;
    }

    if (!m_resourceInitialized) {
        const uint32_t initResult = InitQHYCCDResource();
        if (initResult != QHYCCD_SUCCESS) {
            return setLastError(QStringLiteral("InitQHYCCDResource failed: %1").arg(initResult));
        }
        m_resourceInitialized = true;
    }

    if (!openFirstCamera()) {
        return false;
    }

    if (!applyInitialConfiguration()) {
        close();
        return false;
    }

    if (!syncStateFromCamera()) {
        close();
        return false;
    }

    m_state.connected = true;
    m_state.lastError.clear();
    refreshCapabilities();
    return true;
#else
    m_state.connected = true;
    m_state.frameSpec = sanitizeFrameSpec(m_state.frameSpec);
    m_state.lastError.clear();
    m_capabilities = CameraCapabilities();
    m_capabilities.exposureSupported = true;
    m_capabilities.gainSupported = true;
    m_capabilities.offsetSupported = true;
    m_capabilities.coolerSupported = true;
    m_capabilities.targetTemperatureSupported = true;
    m_capabilities.roiSupported = true;
    m_capabilities.binningSupported = true;
    m_capabilities.frameTypeSupported = true;
    m_capabilities.maxBinX = 4;
    m_capabilities.maxBinY = 4;
    m_capabilities.exposureSeconds = {true, 0.001, 3600.0, 0.001, m_state.exposureSeconds, QStringLiteral("s")};
    m_capabilities.gain = {true, 0.0, 100.0, 1.0, static_cast<double>(m_state.gain), QStringLiteral("adu")};
    m_capabilities.offset = {true, 0.0, 255.0, 1.0, static_cast<double>(m_state.offset), QStringLiteral("adu")};
    m_capabilities.targetTemperature = {true, -30.0, 30.0, 0.1, m_state.targetTemperature, QStringLiteral("C")};
    m_capabilities.frameTypes = defaultFrameTypes();
    return true;
#endif
}

void QhyccdCameraSdkBridge::close()
{
#ifdef Q_OS_ANDROID
    if (m_handle) {
        CloseQHYCCD(reinterpret_cast<qhyccd_handle *>(m_handle));
        m_handle = nullptr;
    }

    if (m_resourceInitialized) {
        ReleaseQHYCCDResource();
        m_resourceInitialized = false;
    }
#endif

    m_state = CameraState();
    m_state.cameraName = QStringLiteral("QHYCCD-AndroidBridge");
    m_state.coolerSupported = true;
    m_state.bitDepth = 16;
    m_lastFrameWasLight = true;
    m_cameraId.clear();
    m_cameraModel.clear();
    m_frameBuffer.clear();
    m_capabilities = CameraCapabilities();
    m_capabilities.frameTypes = defaultFrameTypes();
}

bool QhyccdCameraSdkBridge::isOpen() const
{
    return m_state.connected;
}

CameraState QhyccdCameraSdkBridge::state() const
{
    return m_state;
}

CameraCapabilities QhyccdCameraSdkBridge::capabilities() const
{
    return m_capabilities;
}

bool QhyccdCameraSdkBridge::refreshState()
{
    if (!m_state.connected) {
        m_state.lastError = QStringLiteral("QHYCCD bridge is not connected");
        return false;
    }

#ifdef Q_OS_ANDROID
    return syncStateFromCamera();
#else
    if (m_state.exposing) {
        m_state.exposing = false;
        m_state.frameReady = true;
        m_state.exposureState = CameraExposureState::ReadingOut;
    } else if (m_state.exposureState == CameraExposureState::ReadingOut) {
        m_state.exposureState = CameraExposureState::Idle;
    }

    if (m_state.coolerEnabled) {
        if (m_state.sensorTemperature > m_state.targetTemperature) {
            m_state.sensorTemperature -= 0.3;
        }
    } else if (m_state.sensorTemperature < 20.0) {
        m_state.sensorTemperature += 0.2;
    }

    return true;
#endif
}

bool QhyccdCameraSdkBridge::startExposure(double seconds, bool lightFrame)
{
    if (!m_state.connected) {
        m_state.lastError = QStringLiteral("QHYCCD bridge is not connected");
        m_state.exposureState = CameraExposureState::Error;
        return false;
    }
    if (seconds <= 0.0) {
        m_state.lastError = QStringLiteral("Exposure seconds must be positive");
        m_state.exposureState = CameraExposureState::Error;
        return false;
    }

#ifdef Q_OS_ANDROID
    const uint32_t exposureUs = static_cast<uint32_t>(seconds * 1000000.0);
    if (SetQHYCCDParam(reinterpret_cast<qhyccd_handle *>(m_handle), CONTROL_EXPOSURE, exposureUs) != QHYCCD_SUCCESS) {
        m_state.exposureState = CameraExposureState::Error;
        return setLastError(QStringLiteral("SetQHYCCDParam(CONTROL_EXPOSURE) failed"));
    }

    if (ExpQHYCCDSingleFrame(reinterpret_cast<qhyccd_handle *>(m_handle)) != QHYCCD_SUCCESS) {
        m_state.exposureState = CameraExposureState::Error;
        return setLastError(QStringLiteral("ExpQHYCCDSingleFrame failed"));
    }
#endif

    m_state.exposing = true;
    m_state.frameReady = false;
    m_state.exposureSeconds = seconds;
    m_state.exposureState = CameraExposureState::Exposing;
    m_state.lastError.clear();
    m_lastFrameWasLight = lightFrame;
    return true;
}

bool QhyccdCameraSdkBridge::abortExposure()
{
    if (!m_state.connected) {
        m_state.lastError = QStringLiteral("QHYCCD bridge is not connected");
        return false;
    }

#ifdef Q_OS_ANDROID
    CancelQHYCCDExposingAndReadout(reinterpret_cast<qhyccd_handle *>(m_handle));
#endif

    m_state.exposing = false;
    m_state.frameReady = false;
    m_state.exposureSeconds = 0.0;
    m_state.exposureState = CameraExposureState::Idle;
    return true;
}

bool QhyccdCameraSdkBridge::readoutFrame(QByteArray *buffer)
{
    if (!m_state.connected) {
        m_state.lastError = QStringLiteral("QHYCCD bridge is not connected");
        return false;
    }
    if (!buffer) {
        m_state.lastError = QStringLiteral("Frame buffer is null");
        return false;
    }
    if (!m_state.frameReady) {
        m_state.lastError = QStringLiteral("No QHYCCD frame available");
        return false;
    }

#ifdef Q_OS_ANDROID
    if (!ensureFrameBuffer()) {
        return false;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bpp = 0;
    uint32_t channels = 0;
    const uint32_t result = GetQHYCCDSingleFrame(reinterpret_cast<qhyccd_handle *>(m_handle),
                                                 &width,
                                                 &height,
                                                 &bpp,
                                                 &channels,
                                                 reinterpret_cast<uint8_t *>(m_frameBuffer.data()));
    if (result != QHYCCD_SUCCESS) {
        return setLastError(QStringLiteral("GetQHYCCDSingleFrame failed: %1").arg(result));
    }

    m_state.frameSpec.width = static_cast<int>(width);
    m_state.frameSpec.height = static_cast<int>(height);
    m_state.bitDepth = static_cast<int>(bpp);
    m_state.exposureState = CameraExposureState::Downloading;
    *buffer = m_frameBuffer.left(static_cast<int>((width * height * qMax(1u, channels) * qMax(8u, bpp)) / 8u));
#else
    m_state.exposureState = CameraExposureState::Downloading;
    const QString frameSummary = QStringLiteral("QHYCCD_FRAME:%1:%2x%3:bin=%4x%5:gain=%6:offset=%7")
                                     .arg(m_lastFrameWasLight ? QStringLiteral("LIGHT") : QStringLiteral("DARK"))
                                     .arg(m_state.frameSpec.width)
                                     .arg(m_state.frameSpec.height)
                                     .arg(m_state.frameSpec.binX)
                                     .arg(m_state.frameSpec.binY)
                                     .arg(m_state.gain)
                                     .arg(m_state.offset);
    *buffer = frameSummary.toUtf8();
#endif
    m_state.frameReady = false;
    m_state.exposureState = CameraExposureState::Idle;
    m_state.lastError.clear();
    return true;
}

bool QhyccdCameraSdkBridge::setGain(int gain)
{
#ifdef Q_OS_ANDROID
    if (m_state.connected &&
        SetQHYCCDParam(reinterpret_cast<qhyccd_handle *>(m_handle), CONTROL_GAIN, qMax(0, gain)) != QHYCCD_SUCCESS) {
        return setLastError(QStringLiteral("SetQHYCCDParam(CONTROL_GAIN) failed"));
    }
#endif
    m_state.gain = qMax(0, gain);
    m_capabilities.gain.current = m_state.gain;
    return true;
}

bool QhyccdCameraSdkBridge::setOffset(int offset)
{
#ifdef Q_OS_ANDROID
    if (m_state.connected &&
        SetQHYCCDParam(reinterpret_cast<qhyccd_handle *>(m_handle), CONTROL_OFFSET, qMax(0, offset)) != QHYCCD_SUCCESS) {
        return setLastError(QStringLiteral("SetQHYCCDParam(CONTROL_OFFSET) failed"));
    }
#endif
    m_state.offset = qMax(0, offset);
    m_capabilities.offset.current = m_state.offset;
    return true;
}

bool QhyccdCameraSdkBridge::setFrameSpec(const CameraFrameSpec &spec)
{
    const CameraFrameSpec sanitized = sanitizeFrameSpec(spec);
#ifdef Q_OS_ANDROID
    if (m_state.connected) {
        if (SetQHYCCDBinMode(reinterpret_cast<qhyccd_handle *>(m_handle),
                             static_cast<uint32_t>(sanitized.binX),
                             static_cast<uint32_t>(sanitized.binY)) != QHYCCD_SUCCESS) {
            return setLastError(QStringLiteral("SetQHYCCDBinMode failed"));
        }

        if (SetQHYCCDResolution(reinterpret_cast<qhyccd_handle *>(m_handle),
                                static_cast<uint32_t>(sanitized.x),
                                static_cast<uint32_t>(sanitized.y),
                                static_cast<uint32_t>(sanitized.width),
                                static_cast<uint32_t>(sanitized.height)) != QHYCCD_SUCCESS) {
            return setLastError(QStringLiteral("SetQHYCCDResolution failed"));
        }
    }
#endif
    m_state.frameSpec = sanitized;
    return true;
}

bool QhyccdCameraSdkBridge::setFrameType(CameraFrameType frameType)
{
    m_state.frameType = frameType;
    return true;
}

bool QhyccdCameraSdkBridge::setCoolerEnabled(bool enabled)
{
    if (enabled && !m_state.coolerSupported) {
        m_state.lastError = QStringLiteral("QHYCCD cooler is not supported");
        return false;
    }

#ifdef Q_OS_ANDROID
    if (m_state.connected && enabled) {
        ControlQHYCCDTemp(reinterpret_cast<qhyccd_handle *>(m_handle), m_state.targetTemperature);
    }
#endif
    m_state.coolerEnabled = enabled;
    m_capabilities.coolerSupported = m_state.coolerSupported;
    return true;
}

bool QhyccdCameraSdkBridge::setTargetTemperature(double celsius)
{
    m_state.targetTemperature = celsius;
    m_capabilities.targetTemperature.current = celsius;
#ifdef Q_OS_ANDROID
    if (m_state.connected && m_state.coolerEnabled) {
        if (ControlQHYCCDTemp(reinterpret_cast<qhyccd_handle *>(m_handle), celsius) != QHYCCD_SUCCESS) {
            return setLastError(QStringLiteral("ControlQHYCCDTemp failed"));
        }
    }
#endif
    return true;
}

bool QhyccdCameraSdkBridge::openFirstCamera()
{
#ifdef Q_OS_ANDROID
    QString lastUsbState;
    QString lastPrimedUsbState;
    for (int attempt = 0; attempt < 8; ++attempt) {
        prepareUsbAccess(2500);
        lastUsbState = describeUsbState();
        if (lastUsbState != lastPrimedUsbState && !lastUsbState.contains(QStringLiteral("permission=false"))) {
            primeSdkWithAndroidUsbHandles();
            lastPrimedUsbState = lastUsbState;
        }
        qInfo() << "QhyccdCameraSdkBridge: scan attempt" << (attempt + 1) << "usbState=" << lastUsbState;

        const uint32_t cameraCount = ScanQHYCCD();
        qInfo() << "QhyccdCameraSdkBridge: ScanQHYCCD returned" << cameraCount;
        if (cameraCount != 0 && cameraCount != static_cast<uint32_t>(QHYCCD_ERROR)) {
            char idBuffer[256] = {0};
            if (GetQHYCCDId(0, idBuffer) != QHYCCD_SUCCESS) {
                return setLastError(QStringLiteral("GetQHYCCDId failed"));
            }

            m_cameraId = QByteArray(idBuffer);
            qInfo() << "QhyccdCameraSdkBridge: opening camera id" << m_cameraId;
            qhyccd_handle *handle = OpenQHYCCD(idBuffer);
            if (!handle) {
                return setLastError(QStringLiteral("OpenQHYCCD failed for %1").arg(QString::fromUtf8(m_cameraId)));
            }

            m_handle = handle;

            char modelBuffer[256] = {0};
            if (GetQHYCCDModel(idBuffer, modelBuffer) == QHYCCD_SUCCESS) {
                m_cameraModel = QByteArray(modelBuffer);
                m_state.cameraName = QString::fromUtf8(m_cameraModel);
            } else {
                m_state.cameraName = QString::fromUtf8(m_cameraId);
            }
            return true;
        }

        // QHY cameras may first enumerate as a loader device, then disappear
        // and re-enumerate after the SDK downloads firmware. Give Android USB
        // permission and enumeration a chance to settle before failing.
        QThread::msleep(800);
    }
    return setLastError(QStringLiteral("No QHYCCD camera detected. USB state: %1").arg(lastUsbState));
#else
    return true;
#endif
}

bool QhyccdCameraSdkBridge::applyInitialConfiguration()
{
#ifdef Q_OS_ANDROID
    qhyccd_handle *handle = reinterpret_cast<qhyccd_handle *>(m_handle);
    uint32_t readModeCount = 0;
    if (GetQHYCCDNumberOfReadModes(handle, &readModeCount) == QHYCCD_SUCCESS && readModeCount > 0) {
        if (SetQHYCCDReadMode(handle, 0) != QHYCCD_SUCCESS) {
            qWarning() << "QhyccdCameraSdkBridge: SetQHYCCDReadMode(0) failed";
        }
    }

    if (SetQHYCCDStreamMode(handle, 0) != QHYCCD_SUCCESS) {
        return setLastError(QStringLiteral("SetQHYCCDStreamMode failed"));
    }
    if (InitQHYCCD(handle) != QHYCCD_SUCCESS) {
        return setLastError(QStringLiteral("InitQHYCCD failed"));
    }

    if (SetQHYCCDDebayerOnOff(handle, false) != QHYCCD_SUCCESS) {
        qInfo() << "QhyccdCameraSdkBridge: SetQHYCCDDebayerOnOff(false) not supported";
    }

    if (IsQHYCCDControlAvailable(handle, CONTROL_TRANSFERBIT) == QHYCCD_SUCCESS) {
        if (SetQHYCCDBitsMode(handle, 16) != QHYCCD_SUCCESS) {
            return setLastError(QStringLiteral("SetQHYCCDBitsMode(16) failed"));
        }
        m_state.bitDepth = 16;
    }

    double chipW = 0.0;
    double chipH = 0.0;
    uint32_t imageW = 0;
    uint32_t imageH = 0;
    double pixelW = 0.0;
    double pixelH = 0.0;
    uint32_t bpp = 16;
    if (GetQHYCCDChipInfo(handle, &chipW, &chipH, &imageW, &imageH, &pixelW, &pixelH, &bpp) == QHYCCD_SUCCESS) {
        m_state.frameSpec.width = static_cast<int>(imageW);
        m_state.frameSpec.height = static_cast<int>(imageH);
        m_state.bitDepth = static_cast<int>(bpp);
    }

    uint32_t effectiveStartX = 0;
    uint32_t effectiveStartY = 0;
    uint32_t effectiveSizeX = 0;
    uint32_t effectiveSizeY = 0;
    if (GetQHYCCDEffectiveArea(handle, &effectiveStartX, &effectiveStartY, &effectiveSizeX, &effectiveSizeY) == QHYCCD_SUCCESS &&
        effectiveSizeX > 0 && effectiveSizeY > 0) {
        m_state.frameSpec.x = static_cast<int>(effectiveStartX);
        m_state.frameSpec.y = static_cast<int>(effectiveStartY);
        m_state.frameSpec.width = static_cast<int>(effectiveSizeX);
        m_state.frameSpec.height = static_cast<int>(effectiveSizeY);
    }

    m_state.coolerSupported =
        IsQHYCCDControlAvailable(handle, CONTROL_COOLER) == QHYCCD_SUCCESS ||
        IsQHYCCDControlAvailable(handle, CONTROL_CURTEMP) == QHYCCD_SUCCESS;
    m_state.hasShutter = IsQHYCCDControlAvailable(handle, CAM_MECHANICALSHUTTER) == QHYCCD_SUCCESS;

    if (IsQHYCCDControlAvailable(handle, CONTROL_USBTRAFFIC) == QHYCCD_SUCCESS) {
        if (SetQHYCCDParam(handle, CONTROL_USBTRAFFIC, 30.0) != QHYCCD_SUCCESS) {
            qWarning() << "QhyccdCameraSdkBridge: SetQHYCCDParam(CONTROL_USBTRAFFIC) failed";
        }
    }

    if (IsQHYCCDControlAvailable(handle, CONTROL_DDR) == QHYCCD_SUCCESS) {
        if (SetQHYCCDParam(handle, CONTROL_DDR, 1.0) != QHYCCD_SUCCESS) {
            qWarning() << "QhyccdCameraSdkBridge: SetQHYCCDParam(CONTROL_DDR) failed";
        }
    }

    setFrameSpec(m_state.frameSpec);
    setGain(m_state.gain);
    setOffset(m_state.offset);
    refreshCapabilities();
    return true;
#else
    return true;
#endif
}

void QhyccdCameraSdkBridge::refreshCapabilities()
{
    m_capabilities = CameraCapabilities();
    m_capabilities.frameTypes = defaultFrameTypes();
    m_capabilities.frameTypeSupported = true;
    m_capabilities.roiSupported = true;
    m_capabilities.binningSupported = true;
    m_capabilities.coolerSupported = m_state.coolerSupported;
    m_capabilities.targetTemperatureSupported = m_state.coolerSupported;
    m_capabilities.maxBinX = 1;
    m_capabilities.maxBinY = 1;

#ifdef Q_OS_ANDROID
    qhyccd_handle *handle = reinterpret_cast<qhyccd_handle *>(m_handle);
    if (!handle) {
        return;
    }

    m_capabilities.exposureSupported = IsQHYCCDControlAvailable(handle, CONTROL_EXPOSURE) == QHYCCD_SUCCESS;
    m_capabilities.gainSupported = IsQHYCCDControlAvailable(handle, CONTROL_GAIN) == QHYCCD_SUCCESS;
    m_capabilities.offsetSupported = IsQHYCCDControlAvailable(handle, CONTROL_OFFSET) == QHYCCD_SUCCESS;
    m_capabilities.coolerSupported =
        IsQHYCCDControlAvailable(handle, CONTROL_COOLER) == QHYCCD_SUCCESS ||
        IsQHYCCDControlAvailable(handle, CONTROL_CURTEMP) == QHYCCD_SUCCESS;
    m_capabilities.targetTemperatureSupported = IsQHYCCDControlAvailable(handle, CONTROL_COOLER) == QHYCCD_SUCCESS;

    m_capabilities.exposureSeconds = queryRange(handle, CONTROL_EXPOSURE, m_state.exposureSeconds, 1000000.0, QStringLiteral("s"));
    m_capabilities.gain = queryRange(handle, CONTROL_GAIN, m_state.gain, 1.0, QStringLiteral("adu"));
    m_capabilities.offset = queryRange(handle, CONTROL_OFFSET, m_state.offset, 1.0, QStringLiteral("adu"));
    m_capabilities.targetTemperature = queryRange(handle, CONTROL_COOLER, m_state.targetTemperature, 1.0, QStringLiteral("C"));
    m_capabilities.exposureSupported = m_capabilities.exposureSeconds.supported || m_capabilities.exposureSupported;
    m_capabilities.gainSupported = m_capabilities.gain.supported || m_capabilities.gainSupported;
    m_capabilities.offsetSupported = m_capabilities.offset.supported || m_capabilities.offsetSupported;
    m_capabilities.targetTemperatureSupported =
        m_capabilities.targetTemperature.supported || m_capabilities.targetTemperatureSupported;
    m_capabilities.coolerSupported = m_capabilities.coolerSupported || m_capabilities.targetTemperatureSupported;

    uint32_t readModeCount = 0;
    if (GetQHYCCDNumberOfReadModes(handle, &readModeCount) == QHYCCD_SUCCESS && readModeCount > 0) {
        m_capabilities.readModesSupported = true;
        for (uint32_t i = 0; i < readModeCount; ++i) {
            char nameBuffer[128] = {0};
            if (GetQHYCCDReadModeName(handle, i, nameBuffer) == QHYCCD_SUCCESS && nameBuffer[0] != '\0') {
                m_capabilities.readModeNames.append(QString::fromUtf8(nameBuffer));
            } else {
                m_capabilities.readModeNames.append(QStringLiteral("Mode %1").arg(i));
            }
        }
    }
#else
    m_capabilities.exposureSupported = true;
    m_capabilities.gainSupported = true;
    m_capabilities.offsetSupported = true;
    m_capabilities.coolerSupported = true;
    m_capabilities.targetTemperatureSupported = true;
    m_capabilities.maxBinX = 4;
    m_capabilities.maxBinY = 4;
    m_capabilities.exposureSeconds = {true, 0.001, 3600.0, 0.001, m_state.exposureSeconds, QStringLiteral("s")};
    m_capabilities.gain = {true, 0.0, 100.0, 1.0, static_cast<double>(m_state.gain), QStringLiteral("adu")};
    m_capabilities.offset = {true, 0.0, 255.0, 1.0, static_cast<double>(m_state.offset), QStringLiteral("adu")};
    m_capabilities.targetTemperature = {true, -30.0, 30.0, 0.1, m_state.targetTemperature, QStringLiteral("C")};
#endif
}

bool QhyccdCameraSdkBridge::syncStateFromCamera()
{
#ifdef Q_OS_ANDROID
    qhyccd_handle *handle = reinterpret_cast<qhyccd_handle *>(m_handle);
    if (!handle) {
        return setLastError(QStringLiteral("QHYCCD handle is null"));
    }

    m_state.sensorTemperature = GetQHYCCDParam(handle, CONTROL_CURTEMP);
    if (m_state.coolerEnabled) {
        ControlQHYCCDTemp(handle, m_state.targetTemperature);
    }
    m_capabilities.coolerSupported = m_state.coolerSupported;
    m_capabilities.targetTemperatureSupported = m_state.coolerSupported;
    m_capabilities.exposureSeconds.current = m_state.exposureSeconds;
    m_capabilities.gain.current = m_state.gain;
    m_capabilities.offset.current = m_state.offset;
    m_capabilities.targetTemperature.current = m_state.targetTemperature;

    m_state.connected = true;
    m_state.lastError.clear();
    if (m_state.exposing) {
        m_state.exposing = false;
        m_state.frameReady = true;
        m_state.exposureState = CameraExposureState::ReadingOut;
    } else if (m_state.exposureState == CameraExposureState::ReadingOut) {
        m_state.exposureState = CameraExposureState::Idle;
    }
    return true;
#else
    return true;
#endif
}

bool QhyccdCameraSdkBridge::ensureFrameBuffer()
{
#ifdef Q_OS_ANDROID
    qhyccd_handle *handle = reinterpret_cast<qhyccd_handle *>(m_handle);
    if (!handle) {
        return setLastError(QStringLiteral("QHYCCD handle is null"));
    }

    const uint32_t memLength = GetQHYCCDMemLength(handle);
    if (memLength == 0 || memLength == static_cast<uint32_t>(QHYCCD_ERROR)) {
        return setLastError(QStringLiteral("GetQHYCCDMemLength failed"));
    }

    if (m_frameBuffer.size() != static_cast<int>(memLength)) {
        m_frameBuffer.resize(static_cast<int>(memLength));
    }
#endif
    return true;
}

bool QhyccdCameraSdkBridge::prepareUsbAccess(int timeoutMs)
{
#ifdef Q_OS_ANDROID
    const bool ready = QJniObject::callStaticMethod<jboolean>(
        "org/qtproject/example/QUARCS_app/QhyUsbHelper",
        "prepareForCameraScan",
        "(I)Z",
        timeoutMs);
    if (!ready) {
        qWarning() << "QhyccdCameraSdkBridge: USB access not ready" << describeUsbState();
    }
    return ready;
#else
    Q_UNUSED(timeoutMs);
    return true;
#endif
}

QString QhyccdCameraSdkBridge::describeUsbState() const
{
#ifdef Q_OS_ANDROID
    const QJniObject summary = QJniObject::callStaticObjectMethod(
        "org/qtproject/example/QUARCS_app/QhyUsbHelper",
        "describeQhyDevices",
        "()Ljava/lang/String;");
    if (summary.isValid()) {
        return summary.toString();
    }
#endif
    return QStringLiteral("unavailable");
}

bool QhyccdCameraSdkBridge::primeSdkWithAndroidUsbHandles()
{
#ifdef Q_OS_ANDROID
    const QJniObject records = QJniObject::callStaticObjectMethod(
        "org/qtproject/example/QUARCS_app/QhyUsbHelper",
        "getQhyDeviceRecords",
        "()Ljava/lang/String;");
    if (!records.isValid()) {
        qWarning() << "QhyccdCameraSdkBridge: failed to fetch Android USB records";
        return false;
    }

    const QStringList lines = records.toString().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    bool primedAny = false;
    for (const QString &line : lines) {
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() != 4) {
            continue;
        }
        bool okVendor = false;
        bool okProduct = false;
        bool okFd = false;
        const int vendorId = parts.at(1).toInt(&okVendor);
        const int productId = parts.at(2).toInt(&okProduct);
        const int fileDescriptor = parts.at(3).toInt(&okFd);
        if (!okVendor || !okProduct || !okFd || fileDescriptor <= 0) {
            continue;
        }

        const uint32_t ret = OSXInitQHYCCDAndroidFirmwareArray(vendorId, productId, fileDescriptor);
        qInfo() << "QhyccdCameraSdkBridge: prime Android USB handle"
                << parts.at(0)
                << "vid=" << QString::number(vendorId, 16)
                << "pid=" << QString::number(productId, 16)
                << "fd=" << fileDescriptor
                << "ret=" << ret;
        primedAny = true;
    }
    return primedAny;
#else
    return true;
#endif
}

bool QhyccdCameraSdkBridge::setLastError(const QString &message)
{
    m_state.lastError = message;
    qWarning() << "QhyccdCameraSdkBridge:" << message;
    return false;
}

CameraFrameSpec QhyccdCameraSdkBridge::sanitizeFrameSpec(const CameraFrameSpec &spec) const
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

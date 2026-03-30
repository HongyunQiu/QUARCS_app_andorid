#include "imagingservice.h"

#include "debuglogservice.h"
#include "hardware/devices/camera_device_interface.h"
#include "hardware/devices/device_registry.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QStandardPaths>
#include <limits>

namespace {

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString exposureStateText(CameraExposureState state)
{
    switch (state) {
    case CameraExposureState::Idle:
        return QStringLiteral("Idle");
    case CameraExposureState::Exposing:
        return QStringLiteral("Exposing");
    case CameraExposureState::ReadingOut:
        return QStringLiteral("ReadingOut");
    case CameraExposureState::Downloading:
        return QStringLiteral("Downloading");
    case CameraExposureState::Error:
        return QStringLiteral("Error");
    }

    return QStringLiteral("Unknown");
}

QString frameTypeText(CameraFrameType frameType)
{
    switch (frameType) {
    case CameraFrameType::Light:
        return QStringLiteral("Light");
    case CameraFrameType::Dark:
        return QStringLiteral("Dark");
    case CameraFrameType::Flat:
        return QStringLiteral("Flat");
    case CameraFrameType::Bias:
        return QStringLiteral("Bias");
    }

    return QStringLiteral("Unknown");
}

QImage buildMonoPreview(const QByteArray &frameData, const CameraState &state, bool stretched)
{
    const int width = qMax(1, state.frameSpec.width);
    const int height = qMax(1, state.frameSpec.height);
    if (frameData.isEmpty() || width <= 0 || height <= 0) {
        return {};
    }

    const int targetWidth = qMin(width, 1280);
    const int targetHeight = qMax(1, (height * targetWidth) / qMax(1, width));
    QImage preview(targetWidth, targetHeight, QImage::Format_Grayscale8);
    if (preview.isNull()) {
        return {};
    }

    const int pixelCount = width * height;
    const int bytesPerPixel = state.bitDepth > 8 ? 2 : 1;
    const int requiredBytes = pixelCount * bytesPerPixel;
    if (frameData.size() < requiredBytes) {
        return {};
    }

    quint16 minValue = 0;
    quint16 maxValue = bytesPerPixel == 1 ? 255 : 65535;
    if (stretched) {
        minValue = std::numeric_limits<quint16>::max();
        maxValue = 0;
        if (bytesPerPixel == 1) {
            const auto *src = reinterpret_cast<const uchar *>(frameData.constData());
            for (int i = 0; i < pixelCount; ++i) {
                const quint16 value = src[i];
                minValue = qMin(minValue, value);
                maxValue = qMax(maxValue, value);
            }
        } else {
            const auto *src = reinterpret_cast<const quint16 *>(frameData.constData());
            for (int i = 0; i < pixelCount; ++i) {
                const quint16 value = src[i];
                minValue = qMin(minValue, value);
                maxValue = qMax(maxValue, value);
            }
        }
        if (maxValue <= minValue) {
            stretched = false;
            minValue = 0;
            maxValue = bytesPerPixel == 1 ? 255 : 65535;
        }
    }

    if (bytesPerPixel == 1) {
        const auto *src = reinterpret_cast<const uchar *>(frameData.constData());
        for (int y = 0; y < targetHeight; ++y) {
            uchar *dst = preview.scanLine(y);
            const int srcY = qMin(height - 1, (y * height) / qMax(1, targetHeight));
            const int srcRowOffset = srcY * width;
            for (int x = 0; x < targetWidth; ++x) {
                const int srcX = qMin(width - 1, (x * width) / qMax(1, targetWidth));
                const quint16 value = src[srcRowOffset + srcX];
                if (stretched) {
                    dst[x] = static_cast<uchar>(((value - minValue) * 255u) / qMax<quint16>(1, maxValue - minValue));
                } else {
                    dst[x] = static_cast<uchar>(value);
                }
            }
        }
    } else {
        const auto *src = reinterpret_cast<const quint16 *>(frameData.constData());
        for (int y = 0; y < targetHeight; ++y) {
            uchar *dst = preview.scanLine(y);
            const int srcY = qMin(height - 1, (y * height) / qMax(1, targetHeight));
            const int srcRowOffset = srcY * width;
            for (int x = 0; x < targetWidth; ++x) {
                const int srcX = qMin(width - 1, (x * width) / qMax(1, targetWidth));
                const quint16 value = src[srcRowOffset + srcX];
                if (stretched) {
                    dst[x] = static_cast<uchar>(((value - minValue) * 255u) / qMax<quint16>(1, maxValue - minValue));
                } else {
                    dst[x] = static_cast<uchar>(value >> 8);
                }
            }
        }
    }
    return preview;
}

QString summarizeFrameStats(const QByteArray &frameData, const CameraState &state)
{
    const int width = qMax(1, state.frameSpec.width);
    const int height = qMax(1, state.frameSpec.height);
    const int pixelCount = width * height;
    if (frameData.isEmpty() || pixelCount <= 0) {
        return QStringLiteral("FrameStats:empty");
    }

    const int bytesPerPixel = state.bitDepth > 8 ? 2 : 1;
    if (frameData.size() < pixelCount * bytesPerPixel) {
        return QStringLiteral("FrameStats:short-buffer:%1").arg(frameData.size());
    }

    quint64 sum = 0;
    quint16 minValue = std::numeric_limits<quint16>::max();
    quint16 maxValue = 0;
    int nonZeroHighBytes = 0;

    if (bytesPerPixel == 1) {
        const auto *src = reinterpret_cast<const uchar *>(frameData.constData());
        for (int i = 0; i < pixelCount; ++i) {
            const quint16 value = src[i];
            minValue = qMin(minValue, value);
            maxValue = qMax(maxValue, value);
            sum += value;
            if (value > 0) {
                ++nonZeroHighBytes;
            }
        }
    } else {
        const auto *src = reinterpret_cast<const quint16 *>(frameData.constData());
        for (int i = 0; i < pixelCount; ++i) {
            const quint16 value = src[i];
            minValue = qMin(minValue, value);
            maxValue = qMax(maxValue, value);
            sum += value;
            if ((value >> 8) > 0) {
                ++nonZeroHighBytes;
            }
        }
    }

    const double mean = pixelCount > 0 ? static_cast<double>(sum) / static_cast<double>(pixelCount) : 0.0;
    return QStringLiteral("FrameStats:min=%1 max=%2 mean=%3 nonZeroHigh=%4/%5")
        .arg(minValue)
        .arg(maxValue)
        .arg(mean, 0, 'f', 2)
        .arg(nonZeroHighBytes)
        .arg(pixelCount);
}

}

ImagingService::ImagingService()
{
    device().connect();
}

bool ImagingService::canHandle(const QString &command) const
{
    return command == QStringLiteral("USBCheck") ||
           command == QStringLiteral("getROIInfo") ||
           command == QStringLiteral("getMainCameraParameters") ||
           command == QStringLiteral("getCameraControlState") ||
           command == QStringLiteral("getCameraCapabilities") ||
           command == QStringLiteral("getCaptureStatus") ||
           command == QStringLiteral("getOriginalImage") ||
           command == QStringLiteral("setCameraGain") ||
           command == QStringLiteral("setCameraOffset") ||
           command == QStringLiteral("setCameraBinning") ||
           command == QStringLiteral("setCameraROI") ||
           command == QStringLiteral("resetCameraROI") ||
           command == QStringLiteral("setCameraFrameType") ||
           command == QStringLiteral("setCameraCooler") ||
           command == QStringLiteral("setCameraTargetTemperature") ||
           command == QStringLiteral("ShowAllImageFolder") ||
           command == QStringLiteral("GetImageFiles") ||
           command == QStringLiteral("ReadImageFile") ||
           command == QStringLiteral("DeleteFile") ||
           command == QStringLiteral("MoveFileToUSB") ||
           command == QStringLiteral("GetDownloadManifest") ||
           command == QStringLiteral("ClearDownloadLinks") ||
           command == QStringLiteral("takeExposure") ||
           command == QStringLiteral("takeExposureBurst") ||
           command == QStringLiteral("abortExposure") ||
           command == QStringLiteral("StartLoopCapture") ||
           command == QStringLiteral("StopLoopCapture");
}

QStringList ImagingService::handleCommand(const QString &command, const QString &payload)
{
    if (command == QStringLiteral("USBCheck")) {
        return {QStringLiteral("USBCheck:%1,%2")
                    .arg(device().state().cameraName)
                    .arg(device().state().connected ? QStringLiteral("1") : QStringLiteral("0"))};
    }

    if (command == QStringLiteral("getROIInfo")) {
        ensureConnected();
        return roiInfo();
    }

    if (command == QStringLiteral("getMainCameraParameters")) {
        ensureConnected();
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("getCameraControlState")) {
        ensureConnected();
        return cameraControlState();
    }

    if (command == QStringLiteral("getCameraCapabilities")) {
        ensureConnected();
        return cameraCapabilityInfo();
    }

    if (command == QStringLiteral("getCaptureStatus")) {
        ensureConnected();
        return captureStatus();
    }

    if (command == QStringLiteral("setCameraGain")) {
        int gain = 0;
        if (!parseIntegerValue(payload, &gain) || !ensureConnected() || !device().setGain(gain)) {
            return {};
        }
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("setCameraOffset")) {
        int offset = 0;
        if (!parseIntegerValue(payload, &offset) || !ensureConnected() || !device().setOffset(offset)) {
            return {};
        }
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("setCameraBinning")) {
        int binning = 1;
        if (!parseIntegerValue(payload, &binning) || !ensureConnected()) {
            return {};
        }

        CameraFrameSpec frame = device().state().frameSpec;
        frame.binX = qMax(1, binning);
        frame.binY = qMax(1, binning);
        if (!device().setFrameSpec(frame)) {
            return {};
        }
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("setCameraROI")) {
        CameraFrameSpec frame = device().state().frameSpec;
        if (!parseFrameSpec(payload, &frame) || !ensureConnected() || !device().setFrameSpec(frame)) {
            return {};
        }
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("resetCameraROI")) {
        CameraFrameSpec frame = device().state().frameSpec;
        frame.x = 0;
        frame.y = 0;
        if (device().state().frameSpec.width <= 0 || device().state().frameSpec.height <= 0) {
            return {};
        }
        if (!ensureConnected() || !device().setFrameSpec(frame)) {
            return {};
        }
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("setCameraFrameType")) {
        CameraFrameType frameType = CameraFrameType::Light;
        if (!parseFrameType(payload, &frameType) || !ensureConnected() || !device().setFrameType(frameType)) {
            return {};
        }
        return cameraControlState();
    }

    if (command == QStringLiteral("setCameraCooler")) {
        bool enabled = false;
        if (!parseBoolValue(payload, &enabled) || !ensureConnected() || !device().setCoolerEnabled(enabled)) {
            return {};
        }
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("setCameraTargetTemperature")) {
        double celsius = 0.0;
        if (!parseDoubleValue(payload, &celsius) || !ensureConnected() || !device().setTargetTemperature(celsius)) {
            return {};
        }
        return cameraParameters() + cameraControlState();
    }

    if (command == QStringLiteral("takeExposure") ||
        command == QStringLiteral("takeExposureBurst")) {
        double exposureSeconds = 0.0;
        bool lightFrame = true;
        if (!parseExposureSeconds(payload, &exposureSeconds, &lightFrame)) {
            return {};
        }

        if (!ensureConnected() || !device().startExposure(exposureSeconds, lightFrame)) {
            return {};
        }

        DebugLogService::instance().logManual(
            QStringLiteral("Camera"),
            QStringLiteral("Start exposure %1s (%2 us) type=%3")
                .arg(exposureSeconds, 0, 'f', 3)
                .arg(static_cast<qulonglong>(exposureSeconds * 1000000.0))
                .arg(frameTypeText(device().state().frameType).toLower()));
        return captureStatus();
    }

    if (command == QStringLiteral("abortExposure")) {
        if (!ensureConnected() || !device().abortExposure()) {
            return {};
        }

        DebugLogService::instance().logManual(QStringLiteral("Camera"), QStringLiteral("Abort exposure"));
        return captureStatus();
    }

    if (command == QStringLiteral("getOriginalImage")) {
        if (!ensureConnected()) {
            return {};
        }

        device().refreshState();
        QByteArray frameData;
        if (!device().readoutFrame(&frameData)) {
            return {};
        }

        DebugLogService::instance().logManual(
            QStringLiteral("Camera"),
            QStringLiteral("Readout frame bytes=%1").arg(frameData.size()));
        DebugLogService::instance().logManual(
            QStringLiteral("Camera"),
            summarizeFrameStats(frameData, device().state()));
        const QString cachePath = writeFrameCache(frameData);
        DebugLogService::instance().logManual(
            QStringLiteral("Camera"),
            QStringLiteral("Cached frame path=%1").arg(cachePath));
        const QByteArray previewPng = buildPreviewPng(frameData, false);
        const QByteArray stretchedPreviewPng = buildPreviewPng(frameData, true);
        DebugLogService::instance().logManual(
            QStringLiteral("Camera"),
            QStringLiteral("Preview PNG bytes=%1").arg(previewPng.size()));
        DebugLogService::instance().logManual(
            QStringLiteral("Camera"),
            QStringLiteral("Preview PNG stretched bytes=%1").arg(stretchedPreviewPng.size()));
        return {
            QStringLiteral("OriginalImage:%1").arg(QString::fromLatin1(previewPng.toBase64())),
            QStringLiteral("OriginalImageStretched:%1").arg(QString::fromLatin1(stretchedPreviewPng.toBase64())),
            QStringLiteral("OriginalImageInfo:%1:%2:%3")
                .arg(frameData.size())
                .arg(QCryptographicHash::hash(frameData, QCryptographicHash::Md5).toHex().constData())
                .arg(cachePath),
            captureStatus().value(0)
        };
    }

    if (command == QStringLiteral("StartLoopCapture")) {
        return {
            QStringLiteral("LoopCapture:true"),
            captureStatus().value(0)
        };
    }

    if (command == QStringLiteral("StopLoopCapture")) {
        return {
            QStringLiteral("LoopCapture:false"),
            captureStatus().value(0)
        };
    }

    if (command == QStringLiteral("ShowAllImageFolder") ||
        command == QStringLiteral("GetImageFiles") ||
        command == QStringLiteral("ReadImageFile") ||
        command == QStringLiteral("DeleteFile") ||
        command == QStringLiteral("MoveFileToUSB") ||
        command == QStringLiteral("GetDownloadManifest") ||
        command == QStringLiteral("ClearDownloadLinks")) {
        return {};
    }

    return {};
}

ICameraDevice &ImagingService::device() const
{
    return DeviceRegistry::cameraDevice();
}

bool ImagingService::ensureConnected()
{
    if (device().state().connected) {
        return true;
    }

    return device().connect();
}

QStringList ImagingService::cameraParameters() const
{
    const CameraState state = device().state();
    const CameraFrameSpec frame = state.frameSpec;
    return {
        QStringLiteral("MainCameraParameters:%1:%2:%3:%4:%5:%6:%7:%8:%9:%10:%11")
            .arg(state.cameraName)
            .arg(boolText(state.connected))
            .arg(state.gain)
            .arg(state.offset)
            .arg(frame.x)
            .arg(frame.y)
            .arg(frame.width)
            .arg(frame.height)
            .arg(frame.binX)
            .arg(frame.binY)
            .arg(state.bitDepth),
        QStringLiteral("CameraCooler:%1:%2:%3")
            .arg(boolText(state.coolerSupported))
            .arg(boolText(state.coolerEnabled))
            .arg(state.sensorTemperature, 0, 'f', 1)
    };
}

QStringList ImagingService::cameraControlState() const
{
    const CameraState state = device().state();
    const CameraFrameSpec frame = state.frameSpec;
    return {
        QStringLiteral("CameraControlState:%1:%2:%3:%4:%5:%6:%7:%8:%9:%10:%11:%12")
            .arg(state.gain)
            .arg(state.offset)
            .arg(frame.x)
            .arg(frame.y)
            .arg(frame.width)
            .arg(frame.height)
            .arg(frame.binX)
            .arg(frame.binY)
            .arg(frameTypeText(state.frameType))
            .arg(boolText(state.coolerSupported))
            .arg(boolText(state.coolerEnabled))
            .arg(state.targetTemperature, 0, 'f', 1),
        QStringLiteral("CameraSensorState:%1:%2:%3")
            .arg(state.sensorTemperature, 0, 'f', 1)
            .arg(state.bitDepth)
            .arg(boolText(state.hasShutter))
    };
}

QStringList ImagingService::cameraCapabilityInfo() const
{
    const CameraCapabilities caps = device().capabilities();
    QStringList response = {
        QStringLiteral("CameraCapabilityFlags:%1:%2:%3:%4:%5:%6:%7:%8:%9:%10:%11")
            .arg(boolText(caps.exposureSupported))
            .arg(boolText(caps.gainSupported))
            .arg(boolText(caps.offsetSupported))
            .arg(boolText(caps.coolerSupported))
            .arg(boolText(caps.targetTemperatureSupported))
            .arg(boolText(caps.roiSupported))
            .arg(boolText(caps.binningSupported))
            .arg(boolText(caps.frameTypeSupported))
            .arg(boolText(caps.readModesSupported))
            .arg(caps.maxBinX)
            .arg(caps.maxBinY)
    };

    const auto appendRange = [&response](const QString &name, const CameraControlRange &range) {
        response.append(
            QStringLiteral("CameraControlRange:%1:%2:%3:%4:%5:%6")
                .arg(name)
                .arg(boolText(range.supported))
                .arg(range.minimum, 0, 'f', 6)
                .arg(range.maximum, 0, 'f', 6)
                .arg(range.step, 0, 'f', 6)
                .arg(range.current, 0, 'f', 6));
    };

    appendRange(QStringLiteral("ExposureSeconds"), caps.exposureSeconds);
    appendRange(QStringLiteral("Gain"), caps.gain);
    appendRange(QStringLiteral("Offset"), caps.offset);
    appendRange(QStringLiteral("TargetTemperature"), caps.targetTemperature);

    response.append(QStringLiteral("CameraFrameTypes:%1").arg(caps.frameTypes.join(QLatin1Char('|'))));
    response.append(QStringLiteral("CameraReadModes:%1").arg(caps.readModeNames.join(QLatin1Char('|'))));
    return response;
}

QStringList ImagingService::captureStatus() const
{
    const CameraState state = device().state();
    return {
        QStringLiteral("CaptureStatus:%1:%2:%3:%4")
            .arg(boolText(state.exposing))
            .arg(exposureStateText(state.exposureState))
            .arg(state.exposureSeconds, 0, 'f', 3)
            .arg(boolText(state.frameReady))
    };
}

QStringList ImagingService::roiInfo() const
{
    const CameraFrameSpec frame = device().state().frameSpec;
    return {
        QStringLiteral("ROIInfo:%1:%2:%3:%4:%5:%6")
            .arg(frame.x)
            .arg(frame.y)
            .arg(frame.width)
            .arg(frame.height)
            .arg(frame.binX)
            .arg(frame.binY)
    };
}

QStringList ImagingService::frameTypeInfo() const
{
    return {
        QStringLiteral("CameraFrameType:%1")
            .arg(frameTypeText(device().state().frameType))
    };
}

QString ImagingService::writeFrameCache(const QByteArray &frameData) const
{
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        return {};
    }

    QDir dir(cacheRoot);
    if (!dir.mkpath(QStringLiteral("camera"))) {
        return {};
    }

    const QString hash = QString::fromLatin1(QCryptographicHash::hash(frameData, QCryptographicHash::Md5).toHex());
    const QString filePath = dir.filePath(QStringLiteral("camera/%1.raw").arg(hash));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(frameData);
    file.close();
    return filePath;
}

QByteArray ImagingService::buildPreviewPng(const QByteArray &frameData, bool stretched) const
{
    const CameraState state = device().state();
    const QImage preview = buildMonoPreview(frameData, state, stretched);
    if (preview.isNull()) {
        return {};
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return {};
    }

    QImageWriter writer(&buffer, "png");
    writer.setCompression(3);
    if (!writer.write(preview)) {
        return {};
    }
    return pngData;
}

bool ImagingService::parseExposureSeconds(const QString &payload, double *seconds, bool *lightFrame) const
{
    if (!seconds || !lightFrame) {
        return false;
    }

    QString normalized = payload.trimmed();
    normalized.replace(QLatin1Char(';'), QLatin1Char(':'));
    normalized.replace(QLatin1Char(','), QLatin1Char(':'));
    const QStringList parts = normalized.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    const CameraFrameType currentFrameType = device().state().frameType;
    *lightFrame = currentFrameType == CameraFrameType::Light || currentFrameType == CameraFrameType::Flat;

    bool ok = false;
    for (const QString &part : parts) {
        const QString token = part.trimmed();
        const double value = token.toDouble(&ok);
        if (ok && value > 0.0) {
            *seconds = value;
            return true;
        }

        if (token.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0) {
            *lightFrame = false;
        } else if (token.compare(QStringLiteral("bias"), Qt::CaseInsensitive) == 0) {
            *lightFrame = false;
        } else if (token.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0) {
            *lightFrame = true;
        } else if (token.compare(QStringLiteral("flat"), Qt::CaseInsensitive) == 0) {
            *lightFrame = true;
        }
    }

    return false;
}

bool ImagingService::parseIntegerValue(const QString &payload, int *value) const
{
    if (!value) {
        return false;
    }

    bool ok = false;
    const int parsed = payload.trimmed().toInt(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

bool ImagingService::parseFrameSpec(const QString &payload, CameraFrameSpec *spec) const
{
    if (!spec) {
        return false;
    }

    const QStringList parts = payload.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (parts.size() != 4) {
        return false;
    }

    bool okX = false;
    bool okY = false;
    bool okW = false;
    bool okH = false;
    const int x = parts.at(0).trimmed().toInt(&okX);
    const int y = parts.at(1).trimmed().toInt(&okY);
    const int w = parts.at(2).trimmed().toInt(&okW);
    const int h = parts.at(3).trimmed().toInt(&okH);
    if (!okX || !okY || !okW || !okH) {
        return false;
    }

    spec->x = x;
    spec->y = y;
    spec->width = w;
    spec->height = h;
    return true;
}

bool ImagingService::parseBoolValue(const QString &payload, bool *value) const
{
    if (!value) {
        return false;
    }

    const QString normalized = payload.trimmed().toLower();
    if (normalized == QStringLiteral("1") ||
        normalized == QStringLiteral("true") ||
        normalized == QStringLiteral("on") ||
        normalized == QStringLiteral("enable") ||
        normalized == QStringLiteral("enabled")) {
        *value = true;
        return true;
    }

    if (normalized == QStringLiteral("0") ||
        normalized == QStringLiteral("false") ||
        normalized == QStringLiteral("off") ||
        normalized == QStringLiteral("disable") ||
        normalized == QStringLiteral("disabled")) {
        *value = false;
        return true;
    }

    return false;
}

bool ImagingService::parseDoubleValue(const QString &payload, double *value) const
{
    if (!value) {
        return false;
    }

    bool ok = false;
    const double parsed = payload.trimmed().toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

bool ImagingService::parseFrameType(const QString &payload, CameraFrameType *frameType) const
{
    if (!frameType) {
        return false;
    }

    const QString normalized = payload.trimmed().toLower();
    if (normalized == QStringLiteral("light")) {
        *frameType = CameraFrameType::Light;
        return true;
    }
    if (normalized == QStringLiteral("dark")) {
        *frameType = CameraFrameType::Dark;
        return true;
    }
    if (normalized == QStringLiteral("flat")) {
        *frameType = CameraFrameType::Flat;
        return true;
    }
    if (normalized == QStringLiteral("bias")) {
        *frameType = CameraFrameType::Bias;
        return true;
    }

    return false;
}

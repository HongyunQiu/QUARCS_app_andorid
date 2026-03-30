#ifndef IMAGINGSERVICE_H
#define IMAGINGSERVICE_H

#include "compatcommandservice.h"
#include "hardware/devices/camera_device_interface.h"

class ICameraDevice;

class ImagingService : public CompatCommandService
{
public:
    ImagingService();

    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;

private:
    ICameraDevice &device() const;
    bool ensureConnected();
    QStringList cameraParameters() const;
    QStringList cameraControlState() const;
    QStringList cameraCapabilityInfo() const;
    QStringList captureStatus() const;
    QStringList roiInfo() const;
    QStringList frameTypeInfo() const;
    QString writeFrameCache(const QByteArray &frameData) const;
    QByteArray buildPreviewPng(const QByteArray &frameData, bool stretched) const;
    bool parseExposureSeconds(const QString &payload, double *seconds, bool *lightFrame) const;
    bool parseIntegerValue(const QString &payload, int *value) const;
    bool parseFrameSpec(const QString &payload, CameraFrameSpec *spec) const;
    bool parseBoolValue(const QString &payload, bool *value) const;
    bool parseDoubleValue(const QString &payload, double *value) const;
    bool parseFrameType(const QString &payload, CameraFrameType *frameType) const;
};

#endif // IMAGINGSERVICE_H

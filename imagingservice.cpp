#include "imagingservice.h"

bool ImagingService::canHandle(const QString &command) const
{
    return command == QStringLiteral("USBCheck") ||
           command == QStringLiteral("getROIInfo") ||
           command == QStringLiteral("getMainCameraParameters") ||
           command == QStringLiteral("getCaptureStatus") ||
           command == QStringLiteral("getOriginalImage") ||
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
    Q_UNUSED(payload)

    if (command == QStringLiteral("USBCheck")) {
        return {QStringLiteral("USBCheck:Null,0")};
    }

    if (command == QStringLiteral("getCaptureStatus")) {
        return {QStringLiteral("CaptureStatus:false")};
    }

    return {};
}

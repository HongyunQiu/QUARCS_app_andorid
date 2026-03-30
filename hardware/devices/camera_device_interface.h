#ifndef CAMERA_DEVICE_INTERFACE_H
#define CAMERA_DEVICE_INTERFACE_H

#include <QByteArray>
#include <QString>
#include <QStringList>

enum class CameraExposureState {
    Idle,
    Exposing,
    ReadingOut,
    Downloading,
    Error
};

enum class CameraFrameType {
    Light,
    Dark,
    Flat,
    Bias
};

struct CameraFrameSpec {
    int x = 0;
    int y = 0;
    int width = 1920;
    int height = 1080;
    int binX = 1;
    int binY = 1;
};

struct CameraControlRange {
    bool supported = false;
    double minimum = 0.0;
    double maximum = 0.0;
    double step = 0.0;
    double current = 0.0;
    QString unit;
};

struct CameraCapabilities {
    bool exposureSupported = true;
    bool gainSupported = false;
    bool offsetSupported = false;
    bool coolerSupported = false;
    bool targetTemperatureSupported = false;
    bool roiSupported = true;
    bool binningSupported = true;
    bool frameTypeSupported = true;
    bool readModesSupported = false;
    int maxBinX = 1;
    int maxBinY = 1;
    CameraControlRange exposureSeconds;
    CameraControlRange gain;
    CameraControlRange offset;
    CameraControlRange targetTemperature;
    QStringList readModeNames;
    QStringList frameTypes;
};

struct CameraState {
    bool connected = false;
    bool exposing = false;
    bool coolerSupported = false;
    bool coolerEnabled = false;
    bool hasShutter = true;
    bool frameReady = false;
    double exposureSeconds = 0.0;
    double sensorTemperature = 20.0;
    double targetTemperature = 0.0;
    int gain = 0;
    int offset = 0;
    int bitDepth = 16;
    CameraFrameSpec frameSpec;
    CameraExposureState exposureState = CameraExposureState::Idle;
    CameraFrameType frameType = CameraFrameType::Light;
    QString cameraName = QStringLiteral("AndroidCamera");
    QString lastError;
};

class ICameraDevice
{
public:
    virtual ~ICameraDevice() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual CameraState state() const = 0;
    virtual CameraCapabilities capabilities() const = 0;
    virtual bool refreshState() = 0;
    virtual bool startExposure(double seconds, bool lightFrame) = 0;
    virtual bool abortExposure() = 0;
    virtual bool readoutFrame(QByteArray *buffer) = 0;
    virtual bool setGain(int gain) = 0;
    virtual bool setOffset(int offset) = 0;
    virtual bool setFrameSpec(const CameraFrameSpec &spec) = 0;
    virtual bool setFrameType(CameraFrameType frameType) = 0;
    virtual bool setCoolerEnabled(bool enabled) = 0;
    virtual bool setTargetTemperature(double celsius) = 0;
};

#endif // CAMERA_DEVICE_INTERFACE_H

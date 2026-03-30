#ifndef FOCUSER_DEVICE_INTERFACE_H
#define FOCUSER_DEVICE_INTERFACE_H

#include <QString>

struct FocuserState {
    bool connected = false;
    bool moving = false;
    int position = 0;
    int targetPosition = 0;
    int minPosition = 0;
    int maxPosition = 10000;
    int speed = 1;
    bool reversed = false;
    int backlash = 0;
    int coarseStepDivisions = 10;
    int stepsPerClick = 50;
    double temperature = 0.0;
    QString lastError;
};

class IFocuserDevice
{
public:
    virtual ~IFocuserDevice() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual FocuserState state() const = 0;
    virtual bool refreshState() = 0;
    virtual bool moveAbsolute(int position) = 0;
    virtual bool moveRelative(bool outward, int steps) = 0;
    virtual bool abort() = 0;
    virtual bool syncPosition(int position) = 0;
    virtual bool setSpeed(int speed) = 0;
    virtual bool setReverse(bool enabled) = 0;
};

#endif // FOCUSER_DEVICE_INTERFACE_H

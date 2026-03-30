#ifndef DEFAULT_FOCUSER_DEVICE_H
#define DEFAULT_FOCUSER_DEVICE_H

#include "hardware/devices/focuser_device_interface.h"

class DefaultFocuserDevice : public IFocuserDevice
{
public:
    bool connect() override;
    void disconnect() override;
    FocuserState state() const override;
    bool refreshState() override;
    bool moveAbsolute(int position) override;
    bool moveRelative(bool outward, int steps) override;
    bool abort() override;
    bool syncPosition(int position) override;
    bool setSpeed(int speed) override;
    bool setReverse(bool enabled) override;

private:
    void ensureConnected();
    int boundedPosition(int position) const;

    FocuserState m_state;
};

#endif // DEFAULT_FOCUSER_DEVICE_H

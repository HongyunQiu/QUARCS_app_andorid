#ifndef QFOCUSER_ADAPTER_H
#define QFOCUSER_ADAPTER_H

#include "hardware/devices/focuser_device_interface.h"

#include <memory>

class ITransport;
class QFocuserCore;

class QFocuserAdapter : public IFocuserDevice
{
public:
    explicit QFocuserAdapter(std::unique_ptr<ITransport> transport);
    ~QFocuserAdapter() override;

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
    std::unique_ptr<QFocuserCore> m_core;
};

#endif // QFOCUSER_ADAPTER_H

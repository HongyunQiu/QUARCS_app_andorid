#ifndef FOCUSERSERVICE_H
#define FOCUSERSERVICE_H

#include "compatcommandservice.h"

class IFocuserDevice;

class FocuserService : public CompatCommandService
{
public:
    FocuserService();

    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;

private:
    IFocuserDevice &device() const;
    bool ensureConnected();
    QStringList focusParameters() const;
    QStringList focusPosition() const;
    QStringList moveSingleStep(const QString &direction, const QString &stepsText);
};

#endif // FOCUSERSERVICE_H

#ifndef FOCUSERSERVICE_H
#define FOCUSERSERVICE_H

#include "compatcommandservice.h"

class FocuserService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // FOCUSERSERVICE_H

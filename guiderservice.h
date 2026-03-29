#ifndef GUIDERSERVICE_H
#define GUIDERSERVICE_H

#include "compatcommandservice.h"

class GuiderService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // GUIDERSERVICE_H

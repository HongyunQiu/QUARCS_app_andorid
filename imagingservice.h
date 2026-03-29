#ifndef IMAGINGSERVICE_H
#define IMAGINGSERVICE_H

#include "compatcommandservice.h"

class ImagingService : public CompatCommandService
{
public:
    bool canHandle(const QString &command) const override;
    QStringList handleCommand(const QString &command, const QString &payload) override;
};

#endif // IMAGINGSERVICE_H

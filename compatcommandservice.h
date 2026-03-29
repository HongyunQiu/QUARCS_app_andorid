#ifndef COMPATCOMMANDSERVICE_H
#define COMPATCOMMANDSERVICE_H

#include <QString>
#include <QStringList>

class CompatCommandService
{
public:
    virtual ~CompatCommandService() = default;

    virtual bool canHandle(const QString &command) const = 0;
    virtual QStringList handleCommand(const QString &command, const QString &payload) = 0;
};

#endif // COMPATCOMMANDSERVICE_H

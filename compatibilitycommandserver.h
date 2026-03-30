#ifndef COMPATIBILITYCOMMANDSERVER_H
#define COMPATIBILITYCOMMANDSERVER_H

#include <QObject>
#include <QSet>

#include "devicegatewayservice.h"
#include "filterwheelservice.h"
#include "focuserservice.h"
#include "guiderservice.h"
#include "imagingservice.h"
#include "mountservice.h"
#include "platesolveservice.h"
#include "scheduleservice.h"

class AppHostService;
class QWebSocket;
class QWebSocketServer;

class CompatibilityCommandServer : public QObject
{
    Q_OBJECT

public:
    explicit CompatibilityCommandServer(AppHostService *appHostService, QObject *parent = nullptr);
    ~CompatibilityCommandServer() override;

    bool start(quint16 preferredPort = 8600);
    quint16 port() const;

signals:
    void closeWebViewRequested();

private:
    void onTextMessageReceived(QWebSocket *socket, const QString &message);
    void sendQtReturn(QWebSocket *socket, const QString &message) const;
    void sendLogMessage(QWebSocket *socket, const QString &message) const;
    void broadcastLogMessage(const QString &message) const;
    QStringList handleVueCommand(const QString &message);
    QStringList handleHostCommand(const QString &command, const QString &payload);

    AppHostService *m_appHostService = nullptr;
    QWebSocketServer *m_server = nullptr;
    QSet<QWebSocket *> m_clients;

    DeviceGatewayService m_deviceGatewayService;
    ImagingService m_imagingService;
    MountService m_mountService;
    GuiderService m_guiderService;
    FocuserService m_focuserService;
    FilterWheelService m_filterWheelService;
    PlateSolveService m_plateSolveService;
    ScheduleService m_scheduleService;
};

#endif // COMPATIBILITYCOMMANDSERVER_H

#include "compatibilitycommandserver.h"

#include "apphostservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocket>
#include <QWebSocketServer>

CompatibilityCommandServer::CompatibilityCommandServer(AppHostService *appHostService, QObject *parent)
    : QObject(parent)
    , m_appHostService(appHostService)
{
}

CompatibilityCommandServer::~CompatibilityCommandServer()
{
    delete m_server;
}

bool CompatibilityCommandServer::start(quint16 preferredPort)
{
    if (m_server) {
        return true;
    }

    m_server = new QWebSocketServer(QStringLiteral("QUARCS Embedded Command Bridge"),
                                    QWebSocketServer::NonSecureMode,
                                    this);

    if (!m_server->listen(QHostAddress::LocalHost, preferredPort)) {
        return false;
    }

    connect(m_server, &QWebSocketServer::newConnection, this, [this]() {
        while (m_server->hasPendingConnections()) {
            QWebSocket *socket = m_server->nextPendingConnection();
            m_clients.insert(socket);

            connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString &message) {
                onTextMessageReceived(socket, message);
            });
            connect(socket, &QWebSocket::disconnected, this, [this, socket]() {
                m_clients.remove(socket);
                socket->deleteLater();
            });
        }
    });

    return true;
}

quint16 CompatibilityCommandServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

void CompatibilityCommandServer::onTextMessageReceived(QWebSocket *socket, const QString &message)
{
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject payload = document.object();
    const QString type = payload.value(QStringLiteral("type")).toString();
    const QString commandMessage = payload.value(QStringLiteral("message")).toString();

    if (type == QStringLiteral("Vue_Command")) {
        const QStringList responses = handleVueCommand(commandMessage);
        for (const QString &response : responses) {
            sendQtReturn(socket, response);
        }
        return;
    }

    if (type == QStringLiteral("Broadcast_Msg") &&
        commandMessage == QStringLiteral("CloseWebView")) {
        emit closeWebViewRequested();
    }
}

void CompatibilityCommandServer::sendQtReturn(QWebSocket *socket, const QString &message) const
{
    QJsonObject response;
    response.insert(QStringLiteral("type"), QStringLiteral("QT_Return"));
    response.insert(QStringLiteral("message"), message);
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}

QStringList CompatibilityCommandServer::handleVueCommand(const QString &message)
{
    const int separatorIndex = message.indexOf(QLatin1Char(':'));
    const QString command = separatorIndex >= 0 ? message.left(separatorIndex) : message;
    const QString payload = separatorIndex >= 0 ? message.mid(separatorIndex + 1) : QString();

    QStringList responses = handleHostCommand(command, payload);
    if (!responses.isEmpty()) {
        return responses;
    }

    CompatCommandService *services[] = {
        &m_deviceGatewayService,
        &m_imagingService,
        &m_mountService,
        &m_guiderService,
        &m_focuserService,
        &m_filterWheelService,
        &m_plateSolveService,
        &m_scheduleService
    };

    for (CompatCommandService *service : services) {
        if (service->canHandle(command)) {
            const QStringList serviceResponses = service->handleCommand(command, payload);
            if (!serviceResponses.isEmpty()) {
                return serviceResponses;
            }
        }
    }

    return {};
}

QStringList CompatibilityCommandServer::handleHostCommand(const QString &command, const QString &payload)
{
    if (!m_appHostService) {
        return {};
    }

    if (command == QStringLiteral("getQTClientVersion")) {
        return {QStringLiteral("QTClientVersion:%1").arg(m_appHostService->appVersion())};
    }

    if (command == QStringLiteral("getTotalVersion")) {
        return {QStringLiteral("TotalVersion:%1").arg(m_appHostService->totalVersion())};
    }

    if (command == QStringLiteral("localMessage") || command == QStringLiteral("reGetLocation")) {
        return {
            QStringLiteral("localMessage:%1:%2:%3")
                .arg(m_appHostService->currentLat(),
                     m_appHostService->currentLong(),
                     m_appHostService->currentLanguage())
        };
    }

    if (command == QStringLiteral("getHotspotName")) {
        return {QStringLiteral("HotspotName:%1").arg(m_appHostService->wifiName())};
    }

    if (command == QStringLiteral("getClientSettings")) {
        const QString autoLocation = m_appHostService->readSetting(QStringLiteral("isAutoLocation"), QStringLiteral("true"));
        return {
            QStringLiteral("isAutoLocation:%1").arg(autoLocation),
            QStringLiteral("localMessage:%1:%2:%3")
                .arg(m_appHostService->currentLat(),
                     m_appHostService->currentLong(),
                     m_appHostService->currentLanguage())
        };
    }

    if (command == QStringLiteral("saveToConfigFile")) {
        const int payloadSeparator = payload.indexOf(QLatin1Char(':'));
        if (payloadSeparator > 0) {
            const QString key = payload.left(payloadSeparator);
            const QString value = payload.mid(payloadSeparator + 1);
            m_appHostService->writeSetting(key, value);
        }
        return {};
    }

    if (command == QStringLiteral("loadSDKVersionAndUSBSerialPath")) {
        return {
            QStringLiteral("SDKVersionAndUSBSerialPath:MainCamera:AndroidBridge:null:Guider:AndroidBridge:null:Focuser:AndroidBridge:null")
        };
    }

    if (command == QStringLiteral("getFocuserParameters")) {
        return {QStringLiteral("FocuserParameters:0:10000:0:10:1")};
    }

    if (command == QStringLiteral("getGPIOsStatus")) {
        return {};
    }

    return {};
}

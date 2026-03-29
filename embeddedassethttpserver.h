#ifndef EMBEDDEDASSETHTTPSERVER_H
#define EMBEDDEDASSETHTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QUrl>
#include <QHash>

class AppHostService;
class QTcpSocket;

class EmbeddedAssetHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit EmbeddedAssetHttpServer(AppHostService *appHostService, QObject *parent = nullptr);

    bool start(quint16 preferredPort = 18080);
    quint16 port() const;
    QUrl baseUrl() const;

private:
    struct TilePackEntry {
        qint64 offset = -1;
        qint64 size = 0;
        QString contentType;
    };

    void handleSocketReadyRead(QTcpSocket *socket);
    QByteArray responseForPath(const QString &requestPath, QString *contentType, int *statusCode) const;
    QByteArray loadStaticAsset(const QString &relativePath, bool *found) const;
    QByteArray loadPackedTileAsset(const QString &relativePath, QString *contentType, bool *found) const;
    void ensurePackedTileIndexLoaded() const;
    QString contentTypeForPath(const QString &path) const;

    AppHostService *m_appHostService = nullptr;
    QTcpServer m_server;
    mutable bool m_packedTileIndexLoaded = false;
    mutable QString m_packedTilePackPath;
    mutable QHash<QString, TilePackEntry> m_packedTileIndex;
};

#endif // EMBEDDEDASSETHTTPSERVER_H

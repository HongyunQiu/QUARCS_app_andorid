#include "embeddedassethttpserver.h"

#include "apphostservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

namespace {

QString normalizeRequestPath(const QString &requestPath)
{
    QString path = requestPath;
    const int queryIndex = path.indexOf(QLatin1Char('?'));
    if (queryIndex >= 0) {
        path = path.left(queryIndex);
    }

    if (path.isEmpty() || path == QStringLiteral("/")) {
        return QStringLiteral("index.html");
    }

    path.remove(0, path.startsWith(QLatin1Char('/')) ? 1 : 0);
    return path;
}

} // namespace

EmbeddedAssetHttpServer::EmbeddedAssetHttpServer(AppHostService *appHostService, QObject *parent)
    : QObject(parent)
    , m_appHostService(appHostService)
{
    connect(&m_server, &QTcpServer::newConnection, this, [this]() {
        while (m_server.hasPendingConnections()) {
            QTcpSocket *socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                handleSocketReadyRead(socket);
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

bool EmbeddedAssetHttpServer::start(quint16 preferredPort)
{
    return m_server.listen(QHostAddress::LocalHost, preferredPort);
}

quint16 EmbeddedAssetHttpServer::port() const
{
    return m_server.serverPort();
}

QUrl EmbeddedAssetHttpServer::baseUrl() const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(m_server.serverPort()));
}

void EmbeddedAssetHttpServer::handleSocketReadyRead(QTcpSocket *socket)
{
    const QByteArray request = socket->readAll();
    const QList<QByteArray> lines = request.split('\n');
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }

    const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
    if (requestLine.size() < 2) {
        socket->disconnectFromHost();
        return;
    }

    const QString method = QString::fromUtf8(requestLine.at(0));
    const QString requestPath = QString::fromUtf8(requestLine.at(1));

    QString contentType = QStringLiteral("text/plain; charset=utf-8");
    int statusCode = 200;
    QByteArray body;

    if (method == QStringLiteral("GET") || method == QStringLiteral("HEAD")) {
        body = responseForPath(requestPath, &contentType, &statusCode);
        if (method == QStringLiteral("HEAD")) {
            body.clear();
        }
    } else {
        statusCode = 405;
        body = QByteArrayLiteral("Method Not Allowed");
    }

    const QByteArray statusText = statusCode == 200
        ? QByteArrayLiteral("OK")
        : (statusCode == 404 ? QByteArrayLiteral("Not Found") : QByteArrayLiteral("Method Not Allowed"));

    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + ' ' + statusText + "\r\n";
    response += "Content-Type: " + contentType.toUtf8() + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;

    socket->write(response);
    socket->disconnectFromHost();
}

QByteArray EmbeddedAssetHttpServer::responseForPath(const QString &requestPath, QString *contentType, int *statusCode) const
{
    if (requestPath == QStringLiteral("/quarcs-runtime-config.js")) {
        *contentType = QStringLiteral("application/javascript; charset=utf-8");
        *statusCode = 200;
        return m_appHostService ? m_appHostService->runtimeConfigScript(port()).toUtf8() : QByteArray();
    }

    const QString relativePath = normalizeRequestPath(requestPath);
    bool found = false;
    QByteArray body = loadStaticAsset(relativePath, &found);
    if (!found && relativePath.startsWith(QStringLiteral("tiles/"))) {
        body = loadPackedTileAsset(relativePath, contentType, &found);
    }

    if (!found && !relativePath.contains(QLatin1Char('.'))) {
        body = loadStaticAsset(QStringLiteral("index.html"), &found);
    }

    *statusCode = found ? 200 : 404;
    *contentType = contentTypeForPath(found ? relativePath : QStringLiteral("txt"));

    if (!found) {
        return QByteArrayLiteral("Not Found");
    }

    return body;
}

QByteArray EmbeddedAssetHttpServer::loadStaticAsset(const QString &relativePath, bool *found) const
{
    const QString cleanPath = QDir::cleanPath(relativePath);
    const QStringList candidates = {
        QStringLiteral("assets:/webui/%1").arg(cleanPath),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../android-sources/assets/webui/%1").arg(cleanPath)),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("android-sources/assets/webui/%1").arg(cleanPath))
    };

    for (const QString &candidate : candidates) {
        QFile file(candidate);
        if (!file.exists()) {
            continue;
        }
        if (file.open(QIODevice::ReadOnly)) {
            *found = true;
            return file.readAll();
        }
    }

    *found = false;
    return QByteArray();
}

QByteArray EmbeddedAssetHttpServer::loadPackedTileAsset(const QString &relativePath, QString *contentType, bool *found) const
{
    ensurePackedTileIndexLoaded();

    const auto it = m_packedTileIndex.constFind(relativePath);
    if (it == m_packedTileIndex.constEnd() || m_packedTilePackPath.isEmpty()) {
        *found = false;
        return QByteArray();
    }

    QFile file(m_packedTilePackPath);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(it->offset)) {
        *found = false;
        return QByteArray();
    }

    QByteArray body = file.read(it->size);
    if (body.size() != it->size) {
        *found = false;
        return QByteArray();
    }

    *contentType = it->contentType.isEmpty() ? contentTypeForPath(relativePath) : it->contentType;
    *found = true;
    return body;
}

void EmbeddedAssetHttpServer::ensurePackedTileIndexLoaded() const
{
    if (m_packedTileIndexLoaded) {
        return;
    }

    const QString baseDir = QCoreApplication::applicationDirPath();
    const QStringList indexCandidates = {
        QStringLiteral("assets:/webui/offline-tiles-index.json"),
        QDir(baseDir).filePath(QStringLiteral("../android-sources/assets/webui/offline-tiles-index.json")),
        QDir(baseDir).filePath(QStringLiteral("android-sources/assets/webui/offline-tiles-index.json"))
    };
    const QStringList packCandidates = {
        QStringLiteral("assets:/webui/offline-tiles.pack"),
        QDir(baseDir).filePath(QStringLiteral("../android-sources/assets/webui/offline-tiles.pack")),
        QDir(baseDir).filePath(QStringLiteral("android-sources/assets/webui/offline-tiles.pack"))
    };

    QByteArray indexBytes;
    for (const QString &candidate : indexCandidates) {
        QFile file(candidate);
        if (!file.exists()) {
            continue;
        }
        if (file.open(QIODevice::ReadOnly)) {
            indexBytes = file.readAll();
            break;
        }
    }

    for (const QString &candidate : packCandidates) {
        QFile file(candidate);
        if (file.exists()) {
            m_packedTilePackPath = candidate;
            break;
        }
    }

    if (indexBytes.isEmpty() || m_packedTilePackPath.isEmpty()) {
        m_packedTileIndexLoaded = true;
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(indexBytes);
    if (!document.isObject()) {
        m_packedTileIndexLoaded = true;
        return;
    }

    const QJsonObject entries = document.object().value(QStringLiteral("entries")).toObject();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const QJsonObject entryObject = it.value().toObject();
        TilePackEntry entry;
        entry.offset = static_cast<qint64>(entryObject.value(QStringLiteral("offset")).toDouble(-1));
        entry.size = static_cast<qint64>(entryObject.value(QStringLiteral("size")).toDouble(0));
        entry.contentType = entryObject.value(QStringLiteral("contentType")).toString();
        if (entry.offset >= 0 && entry.size > 0) {
            m_packedTileIndex.insert(it.key(), entry);
        }
    }

    m_packedTileIndexLoaded = true;
}

QString EmbeddedAssetHttpServer::contentTypeForPath(const QString &path) const
{
    if (path.endsWith(QStringLiteral(".html"))) return QStringLiteral("text/html; charset=utf-8");
    if (path.endsWith(QStringLiteral(".js"))) return QStringLiteral("application/javascript; charset=utf-8");
    if (path.endsWith(QStringLiteral(".css"))) return QStringLiteral("text/css; charset=utf-8");
    if (path.endsWith(QStringLiteral(".json"))) return QStringLiteral("application/json; charset=utf-8");
    if (path.endsWith(QStringLiteral(".svg"))) return QStringLiteral("image/svg+xml");
    if (path.endsWith(QStringLiteral(".png"))) return QStringLiteral("image/png");
    if (path.endsWith(QStringLiteral(".jpg")) || path.endsWith(QStringLiteral(".jpeg"))) return QStringLiteral("image/jpeg");
    if (path.endsWith(QStringLiteral(".wasm"))) return QStringLiteral("application/wasm");
    if (path.endsWith(QStringLiteral(".ico"))) return QStringLiteral("image/x-icon");
    if (path.endsWith(QStringLiteral(".gz"))) return QStringLiteral("application/gzip");
    if (path.endsWith(QStringLiteral(".txt")) || path.endsWith(QStringLiteral(".md"))) return QStringLiteral("text/plain; charset=utf-8");
    return QStringLiteral("application/octet-stream");
}

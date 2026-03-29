#include "serverfinder.h"
#include <QThread>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRandomGenerator>//test 使用两个zip中随机的一个作为文件名称
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocketProtocol>
#include <QFileInfo>

ServerFinder::ServerFinder(QObject *parent)
    : QObject(parent)
    , m_progress(0)
    , m_status("Ready")
    , m_uploadProgress(0)
    , m_uploadStatus("Ready")
    , m_multiPart(nullptr) {
}

ServerFinder::~ServerFinder() {
    if (m_multiPart) {
        m_multiPart->deleteLater();
    }
    cancelWebSocketUpload();
}


void ServerFinder::findServerAddress(int timeout){
    // 将耗时操作移到后台线程
        QThread *workerThread = new QThread();
        ServerFinder *worker = new ServerFinder();
        worker->moveToThread(workerThread); // 将工作对象移到新的线程
            connect(workerThread, &QThread::started, [worker, timeout]() {
                worker->searchForServers(timeout);  // 启动搜索
            });
            connect(worker, &ServerFinder::serverAddressesFound, this,
                    [this](const QStringList &addresses, const QStringList &versions) {
                        serverAddresses = addresses;
                        m_serverVersions = versions;
                        //emit serverVersionsChanged();
                        emit serverAddressesFound(addresses, versions);
                    });
            connect(worker, &ServerFinder::errorOccurred, this, &ServerFinder::errorOccurred);
            connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
            connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
            workerThread->start();  // 启动线程
}

void ServerFinder::searchForServers(int timeout){
    serverAddresses.clear();
    m_serverVersions.clear();
    // 创建UDP套接字
    QUdpSocket socket;
    socket.bind(QHostAddress(QHostAddress::AnyIPv4), 8080, QUdpSocket::ShareAddress);
    // 设置超时时间
    QElapsedTimer t;
    t.start();
    /**/
    // 循环接收消息
    while (t.elapsed() < timeout)
    {
        // 接收消息
        QByteArray datagram;
        QHostAddress senderAddress;
        quint16 senderPort;
        if (socket.hasPendingDatagrams())
        {
            datagram.resize(socket.pendingDatagramSize());
            socket.readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);
            qDebug()<<"ip :"<<senderAddress;
            // 判断消息是否是服务器的回复消息
            if (datagram.contains("Stellarium Shared Memory Service"))//(datagram == "Stellarium Shared Memory Service") // !senderAddress.isNull())
            {
                // 仅当该 IP 地址尚未添加到列表中时才添加
                if (!serverAddresses.contains(senderAddress.toString())) {
                    serverAddresses.append(senderAddress.toString());
                    const QString version = extractVersion(datagram);
                    //m_serverVersions.append(QString(""));//test
                    m_serverVersions.append(version.isEmpty() ? QStringLiteral("Unknown") : version);
                }
                //serverAddresses.append(senderAddress.toString()); // 添加 IP 地址到列表
            }
        }
    }
    // serverAddresses.append("192.168.2.37"); // 如果没有找到有效的地址，返回默认地址
    // serverAddresses.append("292.268.255.254");//test
    // m_serverVersions.append(QString("2.6.4"));//test
    // serverAddresses.append("192.168.1.2");//test
    // m_serverVersions.append(QString("3.4.5"));//test
    // serverAddresses.append("0.0.0.3");//test
    // m_serverVersions.append(QString("0.4.5"));//test
    // serverAddresses.append("0.0.0.4");//test

   /*
   if (serverAddresses.isEmpty()) {
       qDebug() << "findServerAddress | timeout or no valid responses";
       serverAddresses.append("192.168.2.37"); // 如果没有找到有效的地址，返回默认地址
       serverAddresses.append("192.168.255.254");
       serverAddresses.append("192.168.1.2");
       serverAddresses.append("0.0.0.3");
       serverAddresses.append("0.0.0.4");
       serverAddresses.append("0.0.0.5");
       serverAddresses.append("0.0.0.6");
   }
*/
    emit serverAddressesFound(serverAddresses, m_serverVersions);  // 返回结果
}

void ServerFinder::findClose(){
    QThread *workerThread = new QThread();
    ServerFinder *worker=new ServerFinder();
    worker->moveToThread(workerThread); // 将工作对象移到新的线程
    connect(workerThread, &QThread::started, [worker]() {
        worker->searchForCloseServers();  // 启动搜索
    });
    connect(worker, &ServerFinder::serverClose, this, &ServerFinder::serverClose);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    workerThread->start();  // 启动线程
}

void ServerFinder::searchForCloseServers(){
    //serverAddresses.clear();
    // 创建UDP套接字
    QUdpSocket socket;
    socket.bind(QHostAddress::AnyIPv4, 8080, QUdpSocket::ShareAddress);
    while (true)
    {
        // 接收消息
        QByteArray datagram;
        QHostAddress senderAddress;
        quint16 senderPort;
        if (socket.hasPendingDatagrams())
        {
            datagram.resize(socket.pendingDatagramSize());
            socket.readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);
            qDebug() << "Raw datagram bytes(close):" << datagram;
            // 判断消息是否是服务器的回复消息
            if (datagram.contains("CloseWebView"))
            {
                break;
            }
        }
    }
    emit serverClose();  // 返回结果
}

// 新增：加载文件内容
bool ServerFinder::loadFile(const QString &filePath) {
    // 处理 qrc:/img/ 路径（注意：Qt 实际使用 "qrc:" 前缀）
    QString actualPath = filePath;
    if (filePath.startsWith("qrc:/")) {
        actualPath = filePath.mid(3);
    }
    QFile file(actualPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << actualPath << "Error:" << file.errorString();
        return false;
    }
    QTextStream stream(&file);
    m_fileContent = stream.readAll();
    file.close();
    emit fileContentChanged();
    qDebug() << "File loaded from:" << actualPath << "Size:" << m_fileContent.size();
    return true;
}

//在广播信息中筛选盒子当前的版本号Vh
QString ServerFinder::extractVersion(const QByteArray &datagram) const
{
    static const QRegularExpression versionPattern(QStringLiteral("Vh\\s*[:=]\\s*([\\w\\.\\-]+)"),
                                                   QRegularExpression::CaseInsensitiveOption);
    qDebug() << "Raw datagram bytes:" << datagram;//qDebug() << "Datagram as string:" << QString::fromUtf8(datagram);
    const QString payload = QString::fromUtf8(datagram);
    const QRegularExpressionMatch match = versionPattern.match(payload);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    return QString();
}

QStringList ServerFinder::findLocalUpdatePackages(const QStringList &versions) const
{
    QStringList matches;
    if (versions.isEmpty()) {
        return matches;
    }

    const QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (downloadDir.isEmpty()) {
        qDebug() << "findLocalUpdatePackages | downloadDir is empty";
        return matches;
    }

    QDir dir(downloadDir);
    if (!dir.exists()) {
        qDebug() << "findLocalUpdatePackages | directory does not exist:" << downloadDir;
        return matches;
    }

    const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &fileName : files) {
        for (const QString &version : versions) {
            const QString trimmedVersion = version.trimmed();
            if (trimmedVersion.isEmpty()) {
                continue;
            }
            if (fileName.contains(trimmedVersion, Qt::CaseInsensitive)) {
                const QString fullPath = dir.absoluteFilePath(fileName);
                matches << fullPath;
                break; // avoid duplicate entries per file
            }
        }
    }

    qDebug() << "findLocalUpdatePackages | matches:" << matches;
    return matches;
}

//下载大文件
void ServerFinder::startDownload(const QUrl &url)
{
    qDebug() << "Download--- Start download from:" << url;
    if (m_reply) {
        qDebug() << "Download--- Canceling previous download";
        cancelDownload();
    }
    m_currentUrl = url;  // 保存URL
    static const QStringList presetNames = {
        //QStringLiteral("2.6.4.zip"),
        //QStringLiteral("3.8.7.zip")
    };
    if (!presetNames.isEmpty()) {
        const int idx = QRandomGenerator::global()->bounded(presetNames.size());
        m_fileName = presetNames.at(idx);
    } else {
        m_fileName = url.fileName().isEmpty() ? QStringLiteral("downloaded_file") : url.fileName();
    }
    qDebug() << "Download--- m_fileName:" << m_fileName;
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);//APP私有目录 //Download公共目录 DownloadLocation
    // QString filePath = QDir(downloadDir).filePath(m_fileName);
    m_currentFilePath = QDir(downloadDir).filePath(m_fileName);  // 保存完整路径
#ifdef Q_OS_IOS
    QDir().mkpath(downloadDir); // 确保目录存在
#endif
    qDebug() << "Download--- filePath:" << m_currentFilePath;

    // 检查文件是否已存在
    if (QFile::exists(m_currentFilePath)) {
        QFile::remove(m_currentFilePath);
        // emit downloadFinished(m_currentFilePath);
        // return;
    }

    m_file.setFileName(m_currentFilePath);
    if (!m_file.open(QIODevice::WriteOnly)) {
        qDebug() << "无法写入文件，错误：" << m_file.errorString();
        emit downloadErrorOccurred(tr("无法写入文件: %1").arg(m_file.errorString()));
        return;
    }

    QNetworkRequest request(url);
    m_reply.reset(m_manager.get(request));

    connect(m_reply.data(), &QNetworkReply::downloadProgress, this, &ServerFinder::onDownloadProgress);
    connect(m_reply.data(), &QNetworkReply::finished, this, &ServerFinder::onFinished);
    connect(m_reply.data(), &QNetworkReply::readyRead, this, &ServerFinder::onReadyRead);
    connect(m_reply.data(), QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &ServerFinder::onDownloadErrorOccurred);

    if (!m_downloadTimeoutTimer) {
        m_downloadTimeoutTimer = new QTimer(this);
        m_downloadTimeoutTimer->setSingleShot(true);
        connect(m_downloadTimeoutTimer, &QTimer::timeout, this, &ServerFinder::onDownloadTimeout);
    }
    m_lastBytesReceived = 0;
    m_downloadTimeoutTimer->start(5000); // 5秒内无进度则判定超时

    m_status = "Downloading";
    emit statusChanged();
}

void ServerFinder::cancelDownload()
{
    if (m_reply) {
        m_reply->abort();
        m_reply.reset();
    }
    if (m_file.isOpen()) {
        m_file.close();
        m_file.remove();
    }
    m_progress = 0;    
    m_status = "Cancelled";
    // emit progressChanged();
    // 重置状态（可选）
    m_currentUrl = QUrl();
    m_currentFilePath.clear();
    emit statusChanged();
    if (m_downloadTimeoutTimer) {
        m_downloadTimeoutTimer->stop();
    }
}

void ServerFinder::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    qDebug() << "Download--- progress:" << bytesReceived << "/" << bytesTotal;
    if (bytesTotal > 0) {
        m_progress = static_cast<double>(bytesReceived) / bytesTotal;
        // emit progressChanged();
        emit progressChanged(m_progress, bytesReceived, bytesTotal);
    }
    m_lastBytesReceived = bytesReceived;
    if (m_downloadTimeoutTimer) {
        m_downloadTimeoutTimer->start(5000);
    }
}

void ServerFinder::cleanupDownload() {
    qDebug() << "cleanupDownload()" << m_status;
    if (m_downloadTimeoutTimer) {
        m_downloadTimeoutTimer->stop();
    }
    if (m_file.isOpen()) {
        m_file.close();
        if (m_status == "Error") {
            m_file.remove();// 删除未完成的文件
        }
    }
    if (!m_reply.isNull()) {
        m_reply->deleteLater();
        m_reply.reset();
    }
    emit statusChanged();
}

void ServerFinder::onFinished()
{
    qDebug() << "onFinished()";
    if (!m_reply)
    {
        qDebug() << "onFinished()  m_reply null";
        return;
    }
    m_status = (m_reply->error() == QNetworkReply::NoError) ? "Finished" : "Error";
    cleanupDownload();
    if (m_status == "Finished") {
        qDebug() << "onFinished() emit downloadFinished()"<<m_currentFilePath;
        emit downloadFinished(m_currentFilePath);
    }
}

void ServerFinder::onReadyRead()
{
    //qDebug() << "Download--- available:" << m_reply->bytesAvailable();
    if (m_file.isOpen()) {
        m_file.write(m_reply->readAll());
    }
}

void ServerFinder::onDownloadTimeout()
{
    qDebug() << "Download timeout triggered";
    if (m_reply) {
        m_reply->abort();
    }
    if (m_status == "Finished")
        return;
    m_status = "Error";
    cleanupDownload();
    emit downloadErrorOccurred(tr("Download timeout"));
}

void ServerFinder::onDownloadErrorOccurred(QNetworkReply::NetworkError code)
{
    qDebug() << "onDownloadErrorOccurred()" << code;
    QString errorStr;//= m_reply ? m_reply->errorString() : tr("Unknown error (%1)").arg(code);
    switch(code) {
    case QNetworkReply::HostNotFoundError:
        errorStr = tr("Please connect to an available network");//Host not found
        break;
    case QNetworkReply::ConnectionRefusedError:
        errorStr = tr("Connection refused");
        break;
    case QNetworkReply::TimeoutError:
        errorStr = tr("Timeout");
        break;
    default:
        errorStr = m_reply ? m_reply->errorString() : tr("Unknown error (%1)").arg(code);
        break;
    }
    Q_UNUSED(code)
    m_status = "Error";
    cleanupDownload();
    emit downloadErrorOccurred(errorStr);//m_reply ? m_reply->errorString() : tr("Unknown error")
}

void ServerFinder::startUpload(const QUrl &url, const QString &filePath)//, const QString &fieldName"/data/user/0/org.qtproject.example.QUARCS_app/files/QHYCamerasDriver202506031303.zip"
{
    // cleanupUpload();
    QFile *file = new QFile(filePath);
    if (!file->exists()) {
        m_uploadStatus = "Error";
        emit uploadStatusChanged();
        delete file;
        return;
    }
    QNetworkRequest request;
    request.setUrl(url);
    // 构建多部分表单数据
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    QString boundary = multiPart->boundary();
    QString contentType = QString("multipart/form-data; boundary=%1").arg(boundary);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (!file->open(QIODevice::ReadOnly)) {
        m_uploadStatus = "Error";
        emit uploadStatusChanged();
        delete file;
        delete multiPart;
        return;
    }
    // file->open(QIODevice::ReadOnly);
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);
    multiPart->setBoundary(boundary.toUtf8());
    // 发送请求
    m_uploadReply = m_manager.post(request, multiPart);
    multiPart->setParent(m_uploadReply);

    connect(m_uploadReply, &QNetworkReply::uploadProgress,this, &ServerFinder::onUploadProgress);
    connect(m_uploadReply, &QNetworkReply::finished, this, &ServerFinder::onUploadFinished);
    connect(m_uploadReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),this, &ServerFinder::onUploadErrorOccurred);

    // 初始化上传超时检测
    m_lastBytesSent = 0;
    if (!m_uploadTimeoutTimer) {
        m_uploadTimeoutTimer = new QTimer(this);
        m_uploadTimeoutTimer->setSingleShot(true);
        connect(m_uploadTimeoutTimer, &QTimer::timeout, this, &ServerFinder::onUploadTimeout);
    }
    m_uploadTimeoutTimer->start(5000); // 上传5秒超时
    m_uploadStatus = "Uploading";
    emit uploadStatusChanged();
}

void ServerFinder::onUploadTimeout()
{
    if (!m_uploadReply) return;
    m_uploadStatus = "Error";
    cleanupUpload();
    emit uploadErrorOccurred(tr("Upload timeout: 5 seconds"));// 上传5秒超时
}

void ServerFinder::cancelUpload()
{
    if (m_uploadReply) {
        m_uploadReply->abort();
        m_uploadReply->reset();//m_uploadReply->deleteLater();
    }
    if (m_multiPart) {
        m_multiPart->deleteLater();
        m_multiPart = nullptr;
    }
    m_uploadProgress = 0;
    m_uploadStatus = "Cancelled";
    emit uploadStatusChanged();
}

void ServerFinder::startWebSocketUpload(const QUrl &url, const QString &filePath)
{
    if (!url.isValid() || url.isEmpty()) {
        m_uploadStatus = "Error";
        emit uploadStatusChanged();
        emit uploadErrorOccurred(tr("Invalid WebSocket URL"));
        return;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        m_uploadStatus = "Error";
        emit uploadStatusChanged();
        emit uploadErrorOccurred(tr("Upload file not found"));
        return;
    }

    // 停止现有的 WebSocket 上传（如果存在）
    cancelWebSocketUpload();

    if (!m_wsSocket) {
        m_wsSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_wsSocket, &QWebSocket::connected, this, &ServerFinder::onWebSocketConnected);
        connect(m_wsSocket, &QWebSocket::disconnected, this, &ServerFinder::onWebSocketDisconnected);
        connect(m_wsSocket, &QWebSocket::errorOccurred, this, &ServerFinder::onWebSocketError);
        connect(m_wsSocket, &QWebSocket::textMessageReceived, this, &ServerFinder::onWebSocketTextMessageReceived);
    } else if (m_wsSocket->state() != QAbstractSocket::UnconnectedState) {
        m_wsSocket->close();
    }

    m_wsUploadFile.setFileName(filePath);
    if (!m_wsUploadFile.open(QIODevice::ReadOnly)) {
        m_uploadStatus = "Error";
        emit uploadStatusChanged();
        emit uploadErrorOccurred(tr("Failed to open file"));
        return;
    }

    m_wsUploadUrl = url;
    m_wsFileSize = m_wsUploadFile.size();
    m_wsBytesSent = 0;
    m_wsUploadFileName = fileInfo.fileName();
    m_isWebSocketUploading = true;
    m_uploadProgress = 0;

    m_uploadStatus = "Connecting";
    emit uploadStatusChanged();
    emit uploadProgressChanged(m_uploadProgress, 0, m_wsFileSize);

    m_wsSocket->open(url);
}

void ServerFinder::cancelWebSocketUpload()
{
    const bool wasUploading = m_isWebSocketUploading;
    cleanupWebSocketUpload();
    if (wasUploading) {
        m_uploadProgress = 0;
        m_uploadStatus = "Cancelled";
        emit uploadStatusChanged();
    }
}

void ServerFinder::onWebSocketConnected()
{
    if (!m_isWebSocketUploading || !m_wsSocket) {
        return;
    }

    QJsonObject meta;
    meta.insert(QStringLiteral("type"), QStringLiteral("meta"));
    meta.insert(QStringLiteral("name"), m_wsUploadFileName);
    meta.insert(QStringLiteral("size"), static_cast<double>(m_wsFileSize));

    m_wsSocket->sendTextMessage(QString::fromUtf8(QJsonDocument(meta).toJson(QJsonDocument::Compact)));

    m_uploadStatus = "Uploading";
    emit uploadStatusChanged();

    QTimer::singleShot(0, this, &ServerFinder::sendNextWebSocketChunk);
}

void ServerFinder::onWebSocketDisconnected()
{
    if (m_isWebSocketUploading) {
        m_uploadStatus = "Error";
        emit uploadStatusChanged();
        emit uploadErrorOccurred(tr("WebSocket disconnected unexpectedly"));
        cleanupWebSocketUpload(false);
    }
}

void ServerFinder::onWebSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    if (!m_isWebSocketUploading) {
        return;
    }
    m_uploadStatus = "Error";
    emit uploadStatusChanged();
    emit uploadErrorOccurred(m_wsSocket ? m_wsSocket->errorString() : tr("WebSocket error"));
    cleanupWebSocketUpload(false);
}

void ServerFinder::onWebSocketTextMessageReceived(const QString &message)
{
    qDebug() << "WebSocket message:" << message;
}

void ServerFinder::sendNextWebSocketChunk()
{
    if (!m_isWebSocketUploading || !m_wsSocket || !m_wsUploadFile.isOpen()) {
        return;
    }
    if (m_wsSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    const qint64 chunkSize = m_wsChunkSize;
    QByteArray chunk = m_wsUploadFile.read(chunkSize);
    if (chunk.isEmpty()) {
        finalizeWebSocketUpload();
        return;
    }

    m_wsSocket->sendBinaryMessage(chunk);
    m_wsBytesSent += chunk.size();
    m_uploadProgress = (m_wsFileSize > 0) ? static_cast<double>(m_wsBytesSent) / m_wsFileSize : 1.0;
    emit uploadProgressChanged(m_uploadProgress, m_wsBytesSent, m_wsFileSize);

    if (m_wsUploadFile.atEnd()) {
        QTimer::singleShot(0, this, &ServerFinder::finalizeWebSocketUpload);
    } else {
        QTimer::singleShot(0, this, &ServerFinder::sendNextWebSocketChunk);
    }
}

void ServerFinder::finalizeWebSocketUpload()
{
    if (!m_isWebSocketUploading) {
        return;
    }

    if (m_wsSocket && m_wsSocket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject finish;
        finish.insert(QStringLiteral("type"), QStringLiteral("finish"));
        finish.insert(QStringLiteral("name"), m_wsUploadFileName);
        finish.insert(QStringLiteral("size"), static_cast<double>(m_wsFileSize));
        m_wsSocket->sendTextMessage(QString::fromUtf8(QJsonDocument(finish).toJson(QJsonDocument::Compact)));
    }

    cleanupWebSocketUpload();

    m_uploadStatus = "Finished";
    emit uploadStatusChanged();
    emit uploadFinished(tr("WebSocket upload finished"));
}

void ServerFinder::cleanupWebSocketUpload(bool closeSocket)
{
    if (m_wsSocket && closeSocket && m_wsSocket->state() != QAbstractSocket::UnconnectedState) {
        m_wsSocket->close();
    }
    if (m_wsUploadFile.isOpen()) {
        m_wsUploadFile.close();
    }
    m_isWebSocketUploading = false;
    m_wsUploadUrl = QUrl();
    m_wsFileSize = 0;
    m_wsBytesSent = 0;
    m_wsUploadFileName.clear();
}

void ServerFinder::onUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    qDebug() << "Upload--- progress:" << bytesSent << "/" << bytesTotal;
    if (bytesTotal > 0) {
        m_uploadProgress = static_cast<double>(bytesSent) / bytesTotal;
        emit uploadProgressChanged(m_uploadProgress, bytesSent, bytesTotal);
    }
    // 重置超时计时器（只要有进度更新就重置）
    m_lastBytesSent = bytesSent;
    if (m_uploadTimeoutTimer) {
        m_uploadTimeoutTimer->start(5000); // 重置5秒超时
    }
}

void ServerFinder::onUploadFinished()
{
    if (m_uploadReply->error() != QNetworkReply::NoError) {
        return;
    }

    QByteArray responseData = m_uploadReply->readAll();
    QString response = QString::fromUtf8(responseData);

    m_uploadStatus = "Finished";
    cleanupUpload();
    emit uploadFinished(response);
}

void ServerFinder::onUploadErrorOccurred(QNetworkReply::NetworkError code)
{
    qDebug() << "onUploadErrorOccurred()" << code;
    QString errorStr;
    switch(code) {
    case QNetworkReply::HostNotFoundError:
        errorStr = tr("Please connect to an available network");//Host not found
        break;
    case QNetworkReply::ConnectionRefusedError:
        errorStr = tr("Connection refused");
        break;
    case QNetworkReply::TimeoutError:
        errorStr = tr("Timeout");
        break;
    default:
        errorStr = m_reply ? m_reply->errorString() : tr("Unknown error (%1)").arg(code);
        break;
    }
    Q_UNUSED(code)
    m_uploadStatus = "Error";
    cleanupUpload();
    emit uploadErrorOccurred(errorStr);//m_uploadReply->errorString()

}

void ServerFinder::cleanupUpload() {
    qDebug() << "cleanupUpload()" << m_uploadStatus;
    // 停止并清理超时计时器
    if (m_uploadTimeoutTimer) {
        m_uploadTimeoutTimer->stop();
    }
    // 1. 安全清理 m_uploadReply（类似 cleanupDownload 中的 m_reply）
    if (m_uploadReply != nullptr){//(!m_uploadReply.isNull()) {
        m_uploadReply->disconnect();  // 断开所有信号槽，避免后续触发
        m_uploadReply->deleteLater(); // 确保异步删除（QNetworkReply 是 QObject）
        m_uploadReply->reset();        // 重置智能指针
    }
    // 2. 安全清理 m_multiPart（类似 cleanupDownload 中的 m_file）
    if (m_multiPart) {
        m_multiPart->deleteLater();   // QHttpMultiPart 是 QObject，必须异步删除
        m_multiPart = nullptr;        // 显式置空
    }
    // 3. 通知状态变更（类似 cleanupDownload 中的 emit statusChanged()）
    emit uploadStatusChanged();
}

// 获取下载的文件大小 方法
qint64 ServerFinder::getDownloadedFileSize() const
{
    if (QFile::exists(m_currentFilePath)) {
        QFileInfo fileInfo(m_currentFilePath);
        return fileInfo.size();
    }
    return 0;
}

// 检查文件是否存在 方法
bool ServerFinder::downloadedFileExists() const
{
    return QFile::exists(m_currentFilePath);
}


void ServerFinder::clearCache()
{
    // // 1. 清理下载相关的缓存
    // cleanupDownload();

    // // 2. 清理上传相关的缓存
    // cleanupUpload();

    // 3. 清理应用缓存目录
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir(cacheDir).removeRecursively();
    QDir().mkpath(cacheDir);

    // // 4. 清理应用数据目录中的临时文件
    // QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // QDir(dataDir).removeRecursively();
    // QDir().mkpath(dataDir);

    // // 5. 重置相关状态
    // m_currentFilePath.clear();
    // m_fileContent.clear();
    // emit currentFilePathChanged();
    // emit fileContentChanged();

    qDebug() << "Cache cleared successfully";
}

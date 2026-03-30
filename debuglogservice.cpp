#include "debuglogservice.h"

#include <QDateTime>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <cstdlib>
#include <cstdio>

namespace {

QMutex &logMutex()
{
    static QMutex mutex;
    return mutex;
}

QString levelLabel(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }

    return QStringLiteral("LOG");
}

QString formatEntry(const QString &channel, const QString &message)
{
    return QStringLiteral("[%1] [%2] %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(channel, message);
}

} // namespace

DebugLogService::DebugLogService(QObject *parent)
    : QObject(parent)
{
}

DebugLogService &DebugLogService::instance()
{
    static DebugLogService service;
    return service;
}

void DebugLogService::installMessageHandler()
{
    qInstallMessageHandler(&DebugLogService::messageHandler);
}

QStringList DebugLogService::recentEntries() const
{
    QMutexLocker locker(&logMutex());
    return m_entries;
}

void DebugLogService::logManual(const QString &category, const QString &message)
{
    appendEntry(formatEntry(category, message));
}

void DebugLogService::appendQueued(const QString &entry)
{
    appendEntry(entry);
}

void DebugLogService::appendEntry(const QString &entry)
{
    {
        QMutexLocker locker(&logMutex());
        m_entries.append(entry);
        while (m_entries.size() > 400) {
            m_entries.removeFirst();
        }
    }

    emit logAppended(entry);
}

void DebugLogService::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString category = context.category && *context.category
        ? QString::fromUtf8(context.category)
        : QStringLiteral("qt");
    const QString entry = formatEntry(levelLabel(type) + QLatin1Char('/') + category, message);

    std::fprintf(stderr, "%s\n", entry.toUtf8().constData());
    std::fflush(stderr);

    QMetaObject::invokeMethod(&DebugLogService::instance(),
                              "appendQueued",
                              Qt::QueuedConnection,
                              Q_ARG(QString, entry));

    if (type == QtFatalMsg) {
        std::abort();
    }
}

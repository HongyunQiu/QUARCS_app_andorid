#ifndef DEBUGLOGSERVICE_H
#define DEBUGLOGSERVICE_H

#include <QObject>
#include <QStringList>

class DebugLogService : public QObject
{
    Q_OBJECT

public:
    static DebugLogService &instance();
    static void installMessageHandler();

    QStringList recentEntries() const;
    void logManual(const QString &category, const QString &message);

signals:
    void logAppended(const QString &entry);

private slots:
    void appendQueued(const QString &entry);

private:
    explicit DebugLogService(QObject *parent = nullptr);
    void appendEntry(const QString &entry);
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message);

    QStringList m_entries;
};

#endif // DEBUGLOGSERVICE_H

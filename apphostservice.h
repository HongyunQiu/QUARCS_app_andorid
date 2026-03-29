#ifndef APPHOSTSERVICE_H
#define APPHOSTSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QSettings>

class WifiInfoProvider;

class AppHostService : public QObject
{
    Q_OBJECT

public:
    explicit AppHostService(const QString &appVersion,
                            WifiInfoProvider *wifiInfoProvider,
                            QObject *parent = nullptr);

    Q_INVOKABLE void updateRuntimeState(const QString &currentTime,
                                        const QString &currentLat,
                                        const QString &currentLong,
                                        const QString &currentLanguage,
                                        const QString &wifiName,
                                        const QString &appVersion = QString());

    QString runtimeConfigScript(quint16 httpPort) const;
    QString appVersion() const;
    QString totalVersion() const;
    QString currentTime() const;
    QString currentLat() const;
    QString currentLong() const;
    QString currentLanguage() const;
    QString wifiName() const;

    QString readSetting(const QString &key, const QString &defaultValue = QString()) const;
    void writeSetting(const QString &key, const QString &value);

private:
    QString prefixedKey(const QString &key) const;
    QJsonObject appStateObject() const;

    QString m_appVersion;
    QString m_totalVersion;
    QString m_currentTime;
    QString m_currentLat;
    QString m_currentLong;
    QString m_currentLanguage;
    QString m_wifiName;
    WifiInfoProvider *m_wifiInfoProvider = nullptr;
    mutable QSettings m_settings;
};

#endif // APPHOSTSERVICE_H

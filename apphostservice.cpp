#include "apphostservice.h"

#include "wifiinfoprovider.h"

#include <QJsonDocument>
#include <QJsonObject>

AppHostService::AppHostService(const QString &appVersion,
                               WifiInfoProvider *wifiInfoProvider,
                               QObject *parent)
    : QObject(parent)
    , m_appVersion(appVersion)
    , m_totalVersion(appVersion)
    , m_currentLanguage(QStringLiteral("en"))
    , m_wifiInfoProvider(wifiInfoProvider)
    , m_settings(QStringLiteral("QUARCS"), QStringLiteral("EmbeddedFrontend"))
{
    if (m_wifiInfoProvider) {
        m_wifiName = m_wifiInfoProvider->wifiInfo();
    }
}

void AppHostService::updateRuntimeState(const QString &currentTime,
                                        const QString &currentLat,
                                        const QString &currentLong,
                                        const QString &currentLanguage,
                                        const QString &wifiName,
                                        const QString &appVersion)
{
    m_currentTime = currentTime;
    m_currentLat = currentLat;
    m_currentLong = currentLong;
    m_currentLanguage = currentLanguage;
    m_wifiName = wifiName;

    if (!appVersion.isEmpty()) {
        m_appVersion = appVersion;
        m_totalVersion = appVersion;
    }
}

QString AppHostService::runtimeConfigScript(quint16 httpPort) const
{
    QJsonObject config;
    config.insert(QStringLiteral("embedded"), true);
    config.insert(QStringLiteral("publicBaseUrl"), QStringLiteral("/"));
    config.insert(QStringLiteral("wsHost"), QStringLiteral("127.0.0.1"));
    config.insert(QStringLiteral("wsPort"), 8600);
    config.insert(QStringLiteral("wsProtocol"), QStringLiteral("ws:"));
    config.insert(QStringLiteral("httpOrigin"), QStringLiteral("http://127.0.0.1:%1").arg(httpPort));
    config.insert(QStringLiteral("enableEngineFonts"), false);
    config.insert(QStringLiteral("appState"), appStateObject());

    const QString json = QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
    return QStringLiteral("window.__QUARCS_RUNTIME_CONFIG__ = Object.assign(%1, window.__QUARCS_RUNTIME_CONFIG__ || {});\n")
        .arg(json);
}

QString AppHostService::appVersion() const
{
    return m_appVersion;
}

QString AppHostService::totalVersion() const
{
    return m_totalVersion;
}

QString AppHostService::currentTime() const
{
    return m_currentTime;
}

QString AppHostService::currentLat() const
{
    return m_currentLat;
}

QString AppHostService::currentLong() const
{
    return m_currentLong;
}

QString AppHostService::currentLanguage() const
{
    return m_currentLanguage.isEmpty() ? QStringLiteral("en") : m_currentLanguage;
}

QString AppHostService::wifiName() const
{
    if (!m_wifiName.isEmpty()) {
        return m_wifiName;
    }

    return m_wifiInfoProvider ? m_wifiInfoProvider->wifiInfo() : QString();
}

QString AppHostService::readSetting(const QString &key, const QString &defaultValue) const
{
    return m_settings.value(prefixedKey(key), defaultValue).toString();
}

void AppHostService::writeSetting(const QString &key, const QString &value)
{
    m_settings.setValue(prefixedKey(key), value);
    m_settings.sync();
}

QString AppHostService::prefixedKey(const QString &key) const
{
    return QStringLiteral("bridge/%1").arg(key);
}

QJsonObject AppHostService::appStateObject() const
{
    QJsonObject state;
    state.insert(QStringLiteral("time"), m_currentTime);
    state.insert(QStringLiteral("lat"), m_currentLat);
    state.insert(QStringLiteral("lon"), m_currentLong);
    state.insert(QStringLiteral("language"), currentLanguage());
    state.insert(QStringLiteral("wifiname"), wifiName());
    state.insert(QStringLiteral("appversion"), m_appVersion);
    return state;
}

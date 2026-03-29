#include "wifiinfoprovider.h"
#include <QDebug>

WifiInfoProvider::WifiInfoProvider(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &WifiInfoProvider::updateWifiInfo);
    m_timer.start();
    //updateWifiInfo();
}

QString WifiInfoProvider::wifiInfo() const
{
    return m_wifiInfo;
}

// void WifiInfoProvider::refresh()
// {
//     updateWifiInfo();
// }

void WifiInfoProvider::updateWifiInfo()
{
    QString newWifiInfo="";

#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        newWifiInfo = QStringLiteral("[Failed to get WiFi]");
    } else {
        QJniObject wifiManager = context.callObjectMethod(
            "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;",
            QJniObject::fromString("wifi").object());
        if (wifiManager.isValid()) {
            QJniObject wifiInfo = wifiManager.callObjectMethod(
                "getConnectionInfo",
                "()Landroid/net/wifi/WifiInfo;");
            if (wifiInfo.isValid()) {
                QString ssid = wifiInfo.callObjectMethod("getSSID", "()Ljava/lang/String;").toString();
                if (!ssid.isEmpty() && ssid != "<unknown ssid>" && ssid != "null") {
                    newWifiInfo = ssid.replace("\"", "");
                } else {
                    newWifiInfo = QStringLiteral("[permission denied]");
                }
            } else {
                newWifiInfo = QStringLiteral("[permission denied]");
            }
        } else {
            newWifiInfo = QStringLiteral("[permission denied]");
        }
    }
#elif defined(Q_OS_IOS)
    QString ssid = getiOSWiFiSSID();
    if (!ssid.isEmpty()) {
        newWifiInfo = ssid;
    } else {
        newWifiInfo = QStringLiteral("[permission denied]");
    }
#endif

    if (newWifiInfo != m_wifiInfo) {
        m_wifiInfo = newWifiInfo;
        qDebug() << "updateWifiInfo change end is:" << m_wifiInfo;
        emit wifiInfoChanged(m_wifiInfo);
    }
}


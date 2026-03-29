#ifndef WIFIINFOPROVIDER_H
#define WIFIINFOPROVIDER_H

#include <QObject>
#include <QString>
#include <QTimer>

#ifdef Q_OS_ANDROID
#include <QPermissions>
#include <QJniObject>// 替代 QAndroidJniObject
#include <QJniEnvironment> // 替代 QAndroidJniEnvironment
#include <QCoreApplication>
#endif

#ifdef Q_OS_IOS
#include "ioswifihelper.h"
#endif

class WifiInfoProvider : public QObject
{
    Q_OBJECT
    //Q_PROPERTY(QString wifiInfo READ wifiInfo NOTIFY wifiInfoChanged) // 确保属性声明
public:
    explicit WifiInfoProvider(QObject *parent = nullptr);

    QString wifiInfo() const;

    Q_INVOKABLE void updateWifiInfo();

signals:
    void wifiInfoChanged(const QString &wifiInfo);

private:

    QString m_wifiInfo = QStringLiteral("WiFi");
    QTimer m_timer;
};

#endif // WIFIINFOPROVIDER_H


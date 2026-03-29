#ifndef IOSWIFIHELPER_H
#define IOSWIFIHELPER_H

#include <QObject>
#ifdef Q_OS_IOS
QString getiOSWiFiSSID(); // 显式声明
#endif

// class IOSWiFiHelper : public QObject
// {
//     Q_OBJECT
// public:
//     explicit IOSWiFiHelper(QObject *parent = nullptr);
//     QString getSSID() const;  // 唯一公开方法，用于获取当前 WiFi 的 SSID

//     // Q_INVOKABLE void requestLocationPermission(std::function<void(bool)> callback);
// };
#endif // IOSWIFIHELPER_H

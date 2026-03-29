#ifndef BLUETOOTHSCANNER_H
#define BLUETOOTHSCANNER_H

#include <QObject>
#include <QString>
#include <QStringList>

#ifdef Q_OS_ANDROID
#include <QPermissions>
#include <QJniObject>
#include <QJniEnvironment>
#include <QCoreApplication>
#include <QThread>
#endif

class BluetoothScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList deviceList READ deviceList NOTIFY deviceListChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit BluetoothScanner(QObject *parent = nullptr);
    ~BluetoothScanner();

    Q_INVOKABLE void startScan(int timeout = 10000);
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE bool checkPermissions();
    Q_INVOKABLE void requestPermissions();

    QStringList deviceList() const { return m_deviceList; }
    QString status() const { return m_status; }

signals:
    void deviceFound(const QString &name, const QString &address);
    void deviceListChanged();
    void scanFinished();
    void scanError(const QString &error);
    void statusChanged();
    void permissionRequestFinished(bool granted);  // 权限请求完成信号

private slots:
    void onScanResult(const QString &name, const QString &address);
    void onScanFinished();
    void onScanError(const QString &error);

private:
    QStringList m_deviceList;
    QString m_status;
    bool m_isScanning;
    int m_pendingScanTimeout;  // 待执行的扫描超时时间（权限授予后使用）

#ifdef Q_OS_ANDROID
    QJniObject m_javaBluetoothScanner;
    void initializeJavaObject();
    void requestPermissionsViaJNI();  // JNI 方式请求权限（作为备用方案）
#endif
};

#endif // BLUETOOTHSCANNER_H


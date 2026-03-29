#ifndef QTBLUETOOTHSCANNER_H
#define QTBLUETOOTHSCANNER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QTimer>
#include <QMap>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>

#ifdef Q_OS_ANDROID
#include <QPermissions>
//#include <qpermissions.h>
//QBluetoothPermission>
#endif

class QtBluetoothScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList deviceList READ deviceList NOTIFY deviceListChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStateChanged)
    Q_PROPERTY(QString connectedDeviceAddress READ connectedDeviceAddress NOTIFY connectionStateChanged)

public:
    explicit QtBluetoothScanner(QObject *parent = nullptr);
    ~QtBluetoothScanner();

    Q_INVOKABLE void startScan(int timeout = 10000);
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE bool checkPermissions();
    Q_INVOKABLE void requestPermissions();
    
    // 连接相关方法
    Q_INVOKABLE void connectToDevice(const QString &address);
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void sendCommand(const QString &command);

    QStringList deviceList() const { return m_deviceList; }
    QString status() const { return m_status; }
    bool isConnected() const { return m_isConnected; }
    QString connectedDeviceAddress() const { return m_connectedDeviceAddress; }

signals:
    void deviceFound(const QString &name, const QString &address);
    void deviceListChanged();
    void scanFinished();
    void scanError(const QString &error);
    void statusChanged();
    void permissionRequestFinished(bool granted);
    
    // 连接相关信号
    void connectionStateChanged(bool connected);
    void serviceDiscovered();
    void characteristicReady();
    void dataReceived(const QByteArray &data);
    void connectionError(const QString &error);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onScanTimeout();
    
    // 连接相关槽函数
    void onControllerStateChanged();
    void onServiceDiscovered(const QBluetoothUuid &serviceUuid);
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);
    void onCharacteristicWrite(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);
    void onControllerError(QLowEnergyController::Error error);

private:
    void updateStatus(const QString &newStatus);
    void checkBluetoothAvailability();
    void enableNotifications();
    QBluetoothDeviceInfo findDeviceByAddress(const QString &address);

    QStringList m_deviceList;
    QString m_status;
    bool m_isScanning;
    int m_pendingScanTimeout;
    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent;
    QBluetoothLocalDevice *m_localDevice;
    QTimer *m_scanTimer;
    
    // 连接相关成员变量
    QMap<QString, QBluetoothDeviceInfo> m_discoveredDevices;  // 存储扫描到的设备信息
    QLowEnergyController *m_controller;
    QLowEnergyService *m_service;
    QLowEnergyCharacteristic m_targetCharacteristic;
    bool m_isConnected;
    QString m_connectedDeviceAddress;
    bool m_writingInProgress;
    qint64 m_lastWriteTime;
    static const qint64 WRITE_INTERVAL = 150;  // ms
    
    // UUID配置（与MyApplication保持一致）
    static const QBluetoothUuid SERVICE_UUID;
    static const QBluetoothUuid CHARACTERISTIC_UUID;
    static const QBluetoothUuid CLIENT_CHARACTERISTIC_CONFIG_UUID;
};

#endif // QTBLUETOOTHSCANNER_H


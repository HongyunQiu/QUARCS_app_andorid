#include "qtbluetoothscanner.h"
#include <QDebug>
#include <QBluetoothAddress>
#include <QBluetoothUuid>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyDescriptor>
#include <QDateTime>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#include <QCoreApplication>
#endif

// 定义UUID常量（与MyApplication保持一致）
const QBluetoothUuid QtBluetoothScanner::SERVICE_UUID = QBluetoothUuid(QUuid("00001234-0000-1000-8000-00805f9b34fb"));
const QBluetoothUuid QtBluetoothScanner::CHARACTERISTIC_UUID = QBluetoothUuid(QUuid("0000abcd-0000-1000-8000-00805f9b34fb"));
const QBluetoothUuid QtBluetoothScanner::CLIENT_CHARACTERISTIC_CONFIG_UUID = QBluetoothUuid(QUuid("00002902-0000-1000-8000-00805f9b34fb"));

QtBluetoothScanner::QtBluetoothScanner(QObject *parent)
    : QObject(parent)
    , m_status("Ready")
    , m_isScanning(false)
    , m_pendingScanTimeout(10000)
    , m_discoveryAgent(nullptr)
    , m_localDevice(nullptr)
    , m_scanTimer(nullptr)
    , m_controller(nullptr)
    , m_service(nullptr)
    , m_isConnected(false)
    , m_writingInProgress(false)
    , m_lastWriteTime(0)
{
    // 初始化本地蓝牙设备
    m_localDevice = new QBluetoothLocalDevice(this);
    
    // 检查蓝牙是否可用
    checkBluetoothAvailability();
    
    // 创建设备发现代理
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    
    // 连接信号和槽
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &QtBluetoothScanner::onDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &QtBluetoothScanner::onScanFinished);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &QtBluetoothScanner::onScanError);
    
    // 创建扫描超时定时器
    m_scanTimer = new QTimer(this);
    m_scanTimer->setSingleShot(true);
    connect(m_scanTimer, &QTimer::timeout, this, &QtBluetoothScanner::onScanTimeout);
    
    qDebug() << "QtBluetoothScanner: Initialized";
}

QtBluetoothScanner::~QtBluetoothScanner()
{
    stopScan();
    disconnectDevice();
    if (m_discoveryAgent) {
        m_discoveryAgent->deleteLater();
    }
    if (m_localDevice) {
        m_localDevice->deleteLater();
    }
    if (m_controller) {
        m_controller->deleteLater();
    }
    if (m_service) {
        m_service->deleteLater();
    }
}

void QtBluetoothScanner::checkBluetoothAvailability()
{
    if (!m_localDevice) {
        updateStatus("Bluetooth not available");
        return;
    }
    
    // 检查蓝牙是否可用
    if (m_localDevice->isValid()) {
        // 确保蓝牙已开启
        if (m_localDevice->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
            qDebug() << "QtBluetoothScanner: Bluetooth is powered off";
            updateStatus("Bluetooth is off");
        } else {
            qDebug() << "QtBluetoothScanner: Bluetooth is available";
            updateStatus("Ready");
        }
    } else {
        qDebug() << "QtBluetoothScanner: Bluetooth adapter not found";
        updateStatus("Bluetooth adapter not found");
    }
}

void QtBluetoothScanner::startScan(int timeout)
{
    qDebug() << "QtBluetoothScanner::startScan: Called with timeout=" << timeout;
    
    if (m_isScanning) {
        qDebug() << "QtBluetoothScanner::startScan: Scan already in progress";
        return;
    }
    
    // 检查权限
    if (!checkPermissions()) {
        qDebug() << "QtBluetoothScanner::startScan: Permissions not granted, requesting...";
        updateStatus("Requesting permissions...");
        m_pendingScanTimeout = timeout;
        requestPermissions();
        return;
    }
    
#ifndef Q_OS_IOS
    // 检查蓝牙可用性（iOS 上 QBluetoothLocalDevice 支持有限，跳过此检查）
    if (!m_localDevice || !m_localDevice->isValid()) {
        qDebug() << "QtBluetoothScanner::startScan: Bluetooth adapter not available";
        updateStatus("Bluetooth adapter not available");
        emit scanError("Bluetooth adapter not available");
        return;
    }
#endif
    
#ifndef Q_OS_IOS
    // 确保蓝牙已开启（iOS 上 QBluetoothLocalDevice 支持有限，跳过此检查）
    if (m_localDevice && m_localDevice->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        qDebug() << "QtBluetoothScanner::startScan: Bluetooth is powered off, attempting to power on";
        m_localDevice->powerOn();
        // 等待一下让蓝牙启动
        QTimer::singleShot(500, this, [this, timeout]() {
            this->startScan(timeout);
        });
        return;
    }
#endif
    
    qDebug() << "QtBluetoothScanner::startScan: Starting device discovery";
    m_isScanning = true;
    m_deviceList.clear();
    updateStatus("Scanning...");
    emit deviceListChanged();
    
    // 设置扫描超时
    if (timeout > 0) {
        m_scanTimer->start(timeout);
    }
    
    // 开始扫描
    // iOS 只支持 BLE，Android 支持经典蓝牙和 BLE
#ifdef Q_OS_IOS
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
#else
    // 使用所有传输方式（经典蓝牙和BLE）
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod | 
                           QBluetoothDeviceDiscoveryAgent::ClassicMethod);
#endif
    
    qDebug() << "QtBluetoothScanner::startScan: Discovery started";
}

void QtBluetoothScanner::stopScan()
{
    if (!m_isScanning) {
        return;
    }
    
    qDebug() << "QtBluetoothScanner::stopScan: Stopping scan";
    
    if (m_discoveryAgent) {
        m_discoveryAgent->stop();
    }
    
    if (m_scanTimer) {
        m_scanTimer->stop();
    }
    
    m_isScanning = false;
    updateStatus("Stopped");
}

bool QtBluetoothScanner::checkPermissions()
{
    qDebug() << "QtBluetoothScanner::checkPermissions: Checking permissions...";
#ifdef Q_OS_ANDROID
    qDebug() << "QtBluetoothScanner::checkPermissions: Android platform detected";
    #if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    QBluetoothPermission bluetoothPermission;
    Qt::PermissionStatus status = qApp->checkPermission(bluetoothPermission);
    qDebug() << "QtBluetoothScanner::checkPermissions: Status=" << status;
    return (status == Qt::PermissionStatus::Granted);
    #else
    // Qt < 6.2，使用 JNI 检查权限
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return false;
    }
    
    QJniObject permissionStr = QJniObject::fromString("android.permission.BLUETOOTH_SCAN");
    if (!permissionStr.isValid()) {
        return false;
    }
    
    QJniEnvironment env;
    jint result = context.callMethod<jint>(
        "checkSelfPermission",
        "(Ljava/lang/String;)I",
        permissionStr.object<jstring>()
    );
    
    if (env.checkAndClearExceptions()) {
        return false;
    }
    
    return (result == 0); // PackageManager.PERMISSION_GRANTED = 0
    #endif
#elif defined(Q_OS_IOS)
    // iOS 平台：权限通过 Info.plist 配置，不需要运行时检查
    // iOS 上 QBluetoothLocalDevice 支持有限，直接返回 true
    // 实际权限会在首次使用蓝牙时由系统弹窗请求
    qDebug() << "QtBluetoothScanner::checkPermissions: *** iOS platform detected ***";
    qDebug() << "QtBluetoothScanner::checkPermissions: *** Returning true for iOS ***";
    return true;
#else
    // 其他平台，通常不需要权限检查
    qDebug() << "QtBluetoothScanner::checkPermissions: Other platform, returning true";
    return true;
#endif
}

void QtBluetoothScanner::requestPermissions()
{
#ifdef Q_OS_ANDROID
    #if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    QBluetoothPermission bluetoothPermission;
    Qt::PermissionStatus status = qApp->checkPermission(bluetoothPermission);
    
    qDebug() << "QtBluetoothScanner::requestPermissions: Current status=" << status;
    
    if (status == Qt::PermissionStatus::Undetermined || status == Qt::PermissionStatus::Denied) {
        qDebug() << "QtBluetoothScanner::requestPermissions: Requesting Bluetooth permission";
        updateStatus("Requesting permissions...");
        
        qApp->requestPermission(bluetoothPermission, [this](const QPermission &permission) {
            Qt::PermissionStatus permStatus = permission.status();
            qDebug() << "QtBluetoothScanner::requestPermissions: Permission result=" << permStatus;
            
            if (permStatus == Qt::PermissionStatus::Granted) {
                updateStatus("Bluetooth permissions granted");
                emit permissionRequestFinished(true);
                
                // 如果有待执行的扫描，自动继续
                if (m_pendingScanTimeout >= 0) {
                    int timeout = m_pendingScanTimeout;
                    m_pendingScanTimeout = -1;
                    QTimer::singleShot(0, this, [this, timeout]() {
                        this->startScan(timeout);
                    });
                }
            } else {
                updateStatus("Bluetooth permissions denied");
                emit permissionRequestFinished(false);
                m_pendingScanTimeout = -1;
                emit scanError("Bluetooth permissions denied. Please grant permissions in settings.");
            }
        });
    } else if (status == Qt::PermissionStatus::Granted) {
        updateStatus("Bluetooth permissions already granted");
        emit permissionRequestFinished(true);
        
        // 如果有待执行的扫描，自动继续
        if (m_pendingScanTimeout >= 0) {
            int timeout = m_pendingScanTimeout;
            m_pendingScanTimeout = -1;
            QTimer::singleShot(0, this, [this, timeout]() {
                this->startScan(timeout);
            });
        }
    }
    #else
    // Qt < 6.2，使用 JNI 请求权限
    qDebug() << "QtBluetoothScanner::requestPermissions: Using JNI method (Qt < 6.2)";
    updateStatus("Requesting permissions...");
    
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;"
    );
    
    if (!activity.isValid()) {
        updateStatus("Error: Failed to get Activity");
        emit permissionRequestFinished(false);
        return;
    }
    
    QJniEnvironment env;
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray permissionsArray = env->NewObjectArray(4, stringClass, nullptr);
    
    QStringList permissionList = {
        "android.permission.BLUETOOTH_SCAN",
        "android.permission.BLUETOOTH_CONNECT",
        "android.permission.ACCESS_FINE_LOCATION",
        "android.permission.ACCESS_COARSE_LOCATION"
    };
    
    for (int i = 0; i < permissionList.size(); ++i) {
        QJniObject perm = QJniObject::fromString(permissionList[i]);
        env->SetObjectArrayElement(permissionsArray, i, perm.object<jstring>());
    }
    
    jint requestCode = 1001;
    activity.callMethod<void>("requestPermissions",
                              "([Ljava/lang/String;I)V",
                              permissionsArray,
                              requestCode);
    
    if (env.checkAndClearExceptions()) {
        updateStatus("Error: Failed to request permissions");
        emit permissionRequestFinished(false);
    } else {
        updateStatus("Permission request sent, waiting for user response");
    }
    
    env->DeleteLocalRef(permissionsArray);
    env->DeleteLocalRef(stringClass);
    #endif
#elif defined(Q_OS_IOS)
    // iOS 平台：权限通过 Info.plist 配置，系统会在首次使用时自动弹窗请求
    // iOS 上 QBluetoothLocalDevice 支持有限，直接返回成功，让扫描继续
    qDebug() << "QtBluetoothScanner::requestPermissions: iOS platform, no runtime request needed";
    updateStatus("Bluetooth ready");
    emit permissionRequestFinished(true);
    
    // 如果有待执行的扫描，自动继续
    if (m_pendingScanTimeout >= 0) {
        int timeout = m_pendingScanTimeout;
        m_pendingScanTimeout = -1;
        QTimer::singleShot(0, this, [this, timeout]() {
            this->startScan(timeout);
        });
    }
#else
    // 其他平台
    updateStatus("Not supported on this platform");
    emit permissionRequestFinished(true);
#endif
}

void QtBluetoothScanner::onDeviceDiscovered(const QBluetoothDeviceInfo &device)
{
    QString name = device.name();
    QString identifier;
    
#ifdef Q_OS_IOS
    // iOS上使用UUID而不是MAC地址（iOS隐私保护机制）
    identifier = device.deviceUuid().toString();
    qDebug() << "QtBluetoothScanner: iOS - Using device UUID:" << identifier;
#else
    // Android和其他平台使用MAC地址
    identifier = device.address().toString();
#endif
    
    // 如果设备名称为空，使用标识符
    if (name.isEmpty()) {
        name = "Unknown Device";
    }
    
    QString deviceInfo = QString("%1 (%2)").arg(name, identifier);
    
    // 避免重复添加
    if (!m_deviceList.contains(deviceInfo)) {
        m_deviceList.append(deviceInfo);
        // 保存设备信息以便后续连接（使用identifier作为key）
        m_discoveredDevices[identifier] = device;
        qDebug() << "QtBluetoothScanner: Device found -" << name << "(" << identifier << ")";
        emit deviceFound(name, identifier);
        emit deviceListChanged();
    }
}

void QtBluetoothScanner::onScanFinished()
{
    qDebug() << "QtBluetoothScanner::onScanFinished: Scan completed";
    
    // 防止重复调用
    if (!m_isScanning) {
        qDebug() << "QtBluetoothScanner::onScanFinished: Already finished, ignoring";
        return;
    }
    
    if (m_scanTimer) {
        m_scanTimer->stop();
    }
    
    m_isScanning = false;
    updateStatus(QString("Found %1 device(s)").arg(m_deviceList.size()));
    emit scanFinished();
}

void QtBluetoothScanner::onScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    QString errorString;
    
    switch (error) {
    case QBluetoothDeviceDiscoveryAgent::NoError:
        return;
    case QBluetoothDeviceDiscoveryAgent::InputOutputError:
        errorString = "Input/Output error";
        break;
    case QBluetoothDeviceDiscoveryAgent::PoweredOffError:
        errorString = "Bluetooth adapter is powered off";
        break;
    case QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError:
        errorString = "Invalid Bluetooth adapter";
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedPlatformError:
        errorString = "Unsupported platform";
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod:
        errorString = "Unsupported discovery method";
        break;
    default:
        errorString = "Unknown error";
        break;
    }
    
    qDebug() << "QtBluetoothScanner::onScanError:" << errorString;
    
    if (m_scanTimer) {
        m_scanTimer->stop();
    }
    
    m_isScanning = false;
    updateStatus("Error: " + errorString);
    emit scanError(errorString);
}

void QtBluetoothScanner::onScanTimeout()
{
    qDebug() << "QtBluetoothScanner::onScanTimeout: Scan timeout reached";
    
    if (!m_isScanning) {
        qDebug() << "QtBluetoothScanner::onScanTimeout: Already finished, ignoring";
        return; // 已经完成或停止了
    }
    
    // 停止扫描，这会触发 finished() 信号，从而调用 onScanFinished()
    if (m_discoveryAgent) {
        qDebug() << "QtBluetoothScanner::onScanTimeout: Stopping discovery agent";
        m_discoveryAgent->stop();
    }
    
    // 如果 stop() 没有立即触发 finished() 信号，我们延迟一点再检查
    // 使用 QTimer::singleShot 确保 onScanFinished 有机会被调用
    QTimer::singleShot(100, this, [this]() {
        if (this->m_isScanning) {
            qDebug() << "QtBluetoothScanner::onScanTimeout: Finished signal not received, manually completing";
            this->m_isScanning = false;
            this->updateStatus(QString("Found %1 device(s)").arg(this->m_deviceList.size()));
            emit this->scanFinished();
        }
    });
}

void QtBluetoothScanner::updateStatus(const QString &newStatus)
{
    if (m_status != newStatus) {
        m_status = newStatus;
        emit statusChanged();
    }
}

// ==================== 连接相关方法实现 ====================

QBluetoothDeviceInfo QtBluetoothScanner::findDeviceByAddress(const QString &address)
{
    // Note: On iOS, 'address' is actually the device UUID due to privacy protection
    // On Android and other platforms, it's the MAC address
    return m_discoveredDevices.value(address);
}

void QtBluetoothScanner::connectToDevice(const QString &address)
{
#ifdef Q_OS_IOS
    qDebug() << "QtBluetoothScanner::connectToDevice: Attempting to connect to device UUID:" << address;
#else
    qDebug() << "QtBluetoothScanner::connectToDevice: Attempting to connect to address:" << address;
#endif
    
    // 检查权限
    if (!checkPermissions()) {
        qDebug() << "QtBluetoothScanner::connectToDevice: Permissions not granted";
        emit connectionError("Bluetooth permissions not granted");
        return;
    }
    
    // 如果已经连接，先断开
    if (m_isConnected && m_controller) {
        qDebug() << "QtBluetoothScanner::connectToDevice: Already connected, disconnecting first";
        disconnectDevice();
    }
    
    // 查找设备信息
    QBluetoothDeviceInfo deviceInfo = findDeviceByAddress(address);
    if (deviceInfo.isValid() == false) {
        qDebug() << "QtBluetoothScanner::connectToDevice: Device not found in discovered devices";
        qDebug() << "QtBluetoothScanner::connectToDevice: Available devices count:" << m_discoveredDevices.size();
#ifdef Q_OS_IOS
        qDebug() << "QtBluetoothScanner::connectToDevice: Looking for UUID:" << address;
#else
        qDebug() << "QtBluetoothScanner::connectToDevice: Looking for address:" << address;
#endif
        // 打印所有已发现的设备标识符用于调试
        for (auto it = m_discoveredDevices.begin(); it != m_discoveredDevices.end(); ++it) {
            qDebug() << "QtBluetoothScanner::connectToDevice: Found device identifier:" << it.key() << "Name:" << it.value().name();
        }
        emit connectionError("Device not found. Please scan first.");
        return;
    }
    
    // 检查设备信息
#ifdef Q_OS_IOS
    qDebug() << "QtBluetoothScanner::connectToDevice: Device found - Name:" << deviceInfo.name() 
             << "UUID:" << deviceInfo.deviceUuid().toString()
             << "CoreConfigurations:" << deviceInfo.coreConfigurations();
#else
    qDebug() << "QtBluetoothScanner::connectToDevice: Device found - Name:" << deviceInfo.name() 
             << "Address:" << deviceInfo.address().toString()
             << "CoreConfigurations:" << deviceInfo.coreConfigurations();
#endif
    
    // 检查设备是否支持低功耗蓝牙
    if (!(deviceInfo.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        qDebug() << "QtBluetoothScanner::connectToDevice: Device does not support BLE";
        emit connectionError("Device does not support Bluetooth Low Energy (BLE)");
        return;
    }
    
    // 创建控制器
    m_controller = QLowEnergyController::createCentral(deviceInfo, this);
    if (!m_controller) {
        qDebug() << "QtBluetoothScanner::connectToDevice: Failed to create controller";
        emit connectionError("Failed to create Bluetooth controller");
        return;
    }
    
    // 连接信号槽
    connect(m_controller, &QLowEnergyController::connected, this, [this, address]() {
        qDebug() << "QtBluetoothScanner: Device connected";
        m_isConnected = true;
#ifdef Q_OS_IOS
        // iOS上存储UUID而不是MAC地址
        m_connectedDeviceAddress = address;
        qDebug() << "QtBluetoothScanner: Connected device UUID:" << address;
#else
        m_connectedDeviceAddress = m_controller->remoteAddress().toString();
        qDebug() << "QtBluetoothScanner: Connected device address:" << m_connectedDeviceAddress;
#endif
        updateStatus("Connected");
        emit connectionStateChanged(true);
        
        // 开始发现服务
        qDebug() << "QtBluetoothScanner: Discovering services...";
        m_controller->discoverServices();
    });
    
    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        qDebug() << "QtBluetoothScanner: Device disconnected";
        m_isConnected = false;
        m_connectedDeviceAddress = "";
        updateStatus("Disconnected");
        emit connectionStateChanged(false);
        
        // 清理资源
        if (m_service) {
            m_service->deleteLater();
            m_service = nullptr;
        }
    });
    
    connect(m_controller, &QLowEnergyController::serviceDiscovered, this, &QtBluetoothScanner::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished, this, [this]() {
        qDebug() << "QtBluetoothScanner: Service discovery finished";
        if (m_service) {
            qDebug() << "QtBluetoothScanner: Service found, discovering details...";
            m_service->discoverDetails();
        } else {
            qDebug() << "QtBluetoothScanner: Target service not found";
            emit connectionError("Service not found");
        }
    });
    
    connect(m_controller, &QLowEnergyController::errorOccurred,
            this, &QtBluetoothScanner::onControllerError);
    
    // 设置连接超时
    QTimer::singleShot(10000, this, [this]() {
        if (m_controller && !m_isConnected) {
            qDebug() << "QtBluetoothScanner::connectToDevice: Connection timeout";
            emit connectionError("Connection timeout. Please ensure the device is nearby and try again.");
            if (m_controller) {
                m_controller->disconnectFromDevice();
            }
        }
    });
    
    // 开始连接
    updateStatus("Connecting...");
    qDebug() << "QtBluetoothScanner::connectToDevice: Calling connectToDevice()";
    m_controller->connectToDevice();
}

void QtBluetoothScanner::disconnectDevice()
{
    if (m_controller) {
        qDebug() << "QtBluetoothScanner::disconnectDevice: Disconnecting...";
        m_controller->disconnectFromDevice();
    }
    
    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }
    
    if (m_controller) {
        m_controller->deleteLater();
        m_controller = nullptr;
    }
    
    m_isConnected = false;
    m_connectedDeviceAddress = "";
    m_targetCharacteristic = QLowEnergyCharacteristic();
    m_writingInProgress = false;
}

void QtBluetoothScanner::sendCommand(const QString &command)
{
    if (!m_isConnected || !m_service) {
        qDebug() << "QtBluetoothScanner::sendCommand: Not connected or service not ready";
        emit connectionError("Not connected to device");
        return;
    }
    
    if (!m_targetCharacteristic.isValid()) {
        qDebug() << "QtBluetoothScanner::sendCommand: Characteristic not valid";
        emit connectionError("Characteristic not ready");
        return;
    }
    
    // 防止写入太快
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_writingInProgress || (now - m_lastWriteTime < WRITE_INTERVAL)) {
        qDebug() << "QtBluetoothScanner::sendCommand: Write in progress or too fast, ignoring";
        return;
    }
    
    m_writingInProgress = true;
    m_lastWriteTime = now;
    
    QByteArray data = command.toUtf8() + "\n";
    qDebug() << "QtBluetoothScanner::sendCommand: Sending command:" << command;
    
    m_service->writeCharacteristic(m_targetCharacteristic, data);
}

void QtBluetoothScanner::onControllerStateChanged()
{
    if (!m_controller) {
        return;
    }
    
    QLowEnergyController::ControllerState state = m_controller->state();
    qDebug() << "QtBluetoothScanner::onControllerStateChanged: State =" << state;
    
    if (state == QLowEnergyController::UnconnectedState) {
        m_isConnected = false;
        m_connectedDeviceAddress = "";
        updateStatus("Disconnected");
        emit connectionStateChanged(false);
    } else if (state == QLowEnergyController::ConnectedState) {
        m_isConnected = true;
        m_connectedDeviceAddress = m_controller->remoteAddress().toString();
        updateStatus("Connected");
        emit connectionStateChanged(true);
    }
}

void QtBluetoothScanner::onServiceDiscovered(const QBluetoothUuid &serviceUuid)
{
    qDebug() << "QtBluetoothScanner::onServiceDiscovered: Service UUID =" << serviceUuid.toString();
    
    if (serviceUuid == SERVICE_UUID) {
        qDebug() << "QtBluetoothScanner::onServiceDiscovered: Target service found!";
        m_service = m_controller->createServiceObject(serviceUuid, this);
        
        if (m_service) {
            // 连接服务信号
            connect(m_service, &QLowEnergyService::stateChanged, this, &QtBluetoothScanner::onServiceStateChanged);
            connect(m_service, &QLowEnergyService::characteristicChanged, this, &QtBluetoothScanner::onCharacteristicChanged);
            connect(m_service, &QLowEnergyService::characteristicWritten, this, &QtBluetoothScanner::onCharacteristicWrite);
            
            emit serviceDiscovered();
        } else {
            qDebug() << "QtBluetoothScanner::onServiceDiscovered: Failed to create service object";
            emit connectionError("Failed to create service object");
        }
    }
}

void QtBluetoothScanner::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    qDebug() << "QtBluetoothScanner::onServiceStateChanged: State =" << state;
    
    if (state == QLowEnergyService::RemoteServiceDiscovered) {
        qDebug() << "QtBluetoothScanner::onServiceStateChanged: Service discovered, getting characteristic";
        
        // 获取目标特征
        m_targetCharacteristic = m_service->characteristic(CHARACTERISTIC_UUID);
        
        if (m_targetCharacteristic.isValid()) {
            qDebug() << "QtBluetoothScanner::onServiceStateChanged: Characteristic found!";
            emit characteristicReady();
            
            // 启用通知
            enableNotifications();
        } else {
            qDebug() << "QtBluetoothScanner::onServiceStateChanged: Characteristic not found";
            emit connectionError("Characteristic not found");
        }
    }
}

void QtBluetoothScanner::enableNotifications()
{
    if (!m_service || !m_targetCharacteristic.isValid()) {
        qDebug() << "QtBluetoothScanner::enableNotifications: Service or characteristic not valid";
        return;
    }
    
    qDebug() << "QtBluetoothScanner::enableNotifications: Enabling notifications";
    
    // 启用特征通知
    QLowEnergyDescriptor descriptor = m_targetCharacteristic.descriptor(CLIENT_CHARACTERISTIC_CONFIG_UUID);
    if (descriptor.isValid()) {
        // 启用通知
        m_service->writeDescriptor(descriptor, QByteArray::fromHex("0100"));
        qDebug() << "QtBluetoothScanner::enableNotifications: Notifications enabled";
    } else {
        qDebug() << "QtBluetoothScanner::enableNotifications: Descriptor not found";
    }
}

void QtBluetoothScanner::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qDebug() << "QtBluetoothScanner::onCharacteristicChanged: Received data:" << value;
    emit dataReceived(value);
}

void QtBluetoothScanner::onCharacteristicWrite(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    Q_UNUSED(characteristic)
    Q_UNUSED(value)
    qDebug() << "QtBluetoothScanner::onCharacteristicWrite: Write completed";
    m_writingInProgress = false;
}

void QtBluetoothScanner::onControllerError(QLowEnergyController::Error error)
{
    if (!m_controller) {
        return;
    }
    QString errorString;
    QString userMessage;
    
    switch (error) {
    case QLowEnergyController::NoError:
        return;
    case QLowEnergyController::UnknownError:
        errorString = "Unknown error";
        userMessage = "Connection failed. Please ensure the device is nearby and try again.";
        break;
    case QLowEnergyController::RemoteHostClosedError:
        errorString = "Remote host closed connection";
        userMessage = "Device disconnected. Please check if the device is still powered on.";
        break;
    case QLowEnergyController::ConnectionError:
        errorString = "Connection error";
        userMessage = "Failed to connect. Possible reasons:\n"
                     "- Device is out of range\n"
                     "- Device is already connected to another app\n"
                     "- Device does not support BLE connection\n"
                     "- Please try scanning again";
        break;
    default:
        errorString = "Error code: " + QString::number(error);
        userMessage = "Connection error (code: " + QString::number(error) + "). Please try again.";
        break;
    }
    
    qDebug() << "QtBluetoothScanner::onControllerError:" << errorString << "Error code:" << error;
    qDebug() << "QtBluetoothScanner::onControllerError: Controller state:" << (m_controller ? m_controller->state() : -1);
    emit connectionError(userMessage);
    updateStatus("Error: " + errorString);
}


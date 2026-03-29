#include "bluetoothscanner.h"
#include <QDebug>
#include <QThread>
#include <QMetaObject>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#include <QGuiApplication>
#include <QCoreApplication>
#include <jni.h>
#endif

BluetoothScanner::BluetoothScanner(QObject *parent)
    : QObject(parent)
    , m_status("Ready")
    , m_isScanning(false)
    , m_pendingScanTimeout(-1)  // -1 表示没有待执行的扫描
{
    // 延迟初始化 Java 对象，不在构造函数中初始化
    // 避免 Android 环境未完全初始化时出现问题
}

BluetoothScanner::~BluetoothScanner()
{
    stopScan();
}

void BluetoothScanner::startScan(int timeout)
{
    qDebug() << "BluetoothScanner::startScan: Called with timeout=" << timeout;
    qDebug() << "BluetoothScanner::startScan: m_isScanning=" << m_isScanning;
    
    if (m_isScanning) {
        qDebug() << "BluetoothScanner::startScan: Scan already in progress, returning";
        return;
    }

    qDebug() << "BluetoothScanner::startScan: Checking permissions...";
    if (!checkPermissions()) {
        qDebug() << "BluetoothScanner::startScan: Permissions not granted, requesting...";
        m_status = "Requesting permissions...";
        emit statusChanged();
        // 保存待执行的扫描参数
        m_pendingScanTimeout = timeout;
        // 自动请求权限，权限授予后会自动继续扫描
        requestPermissions();
        return;
    }
    qDebug() << "BluetoothScanner::startScan: Permissions OK";

#ifdef Q_OS_ANDROID
    // 延迟初始化：在第一次使用时才初始化 Java 对象
    if (!m_javaBluetoothScanner.isValid()) {
        qDebug() << "BluetoothScanner::startScan: Java object not valid, initializing...";
        initializeJavaObject();
        if (!m_javaBluetoothScanner.isValid()) {
            qDebug() << "BluetoothScanner::startScan: Failed to initialize Java BluetoothScanner object";
            m_status = "Error: Java object not initialized";
            emit statusChanged();
            emit scanError("Failed to initialize Bluetooth scanner");
            return;
        }
        qDebug() << "BluetoothScanner::startScan: Java object initialized successfully";
    } else {
        qDebug() << "BluetoothScanner::startScan: Java object already valid";
    }
#endif

    qDebug() << "BluetoothScanner::startScan: Setting scanning state...";
    m_isScanning = true;
    m_deviceList.clear();
    m_status = "Scanning...";
    emit statusChanged();
    emit deviceListChanged();
    qDebug() << "BluetoothScanner::startScan: State set, about to call Java startScan...";

#ifdef Q_OS_ANDROID
    if (m_javaBluetoothScanner.isValid()) {
        qDebug() << "BluetoothScanner::startScan: Creating timeout string...";
        QJniObject timeoutObj = QJniObject::fromString(QString::number(timeout));
        qDebug() << "BluetoothScanner::startScan: Timeout string created, calling Java startScan method...";
        qDebug() << "BluetoothScanner::startScan: Java object pointer=" << reinterpret_cast<void*>(m_javaBluetoothScanner.object());
        
        QJniEnvironment env;
        m_javaBluetoothScanner.callMethod<void>("startScan", "(Ljava/lang/String;)V", timeoutObj.object<jstring>());
        
        if (env.checkAndClearExceptions()) {
            qDebug() << "BluetoothScanner::startScan: Exception occurred calling Java startScan";
            m_isScanning = false;
            m_status = "Error: Failed to start scan";
            emit statusChanged();
            emit scanError("Failed to start Bluetooth scan");
        } else {
            qDebug() << "BluetoothScanner::startScan: Java startScan called successfully";
        }
    } else {
        qDebug() << "BluetoothScanner::startScan: Java BluetoothScanner object is not valid";
        m_isScanning = false;
        m_status = "Error: Java object not initialized";
        emit statusChanged();
        emit scanError("Failed to initialize Bluetooth scanner");
    }
#else
    // 非 Android 平台的模拟实现
    qDebug() << "Bluetooth scanning not supported on this platform";
    m_isScanning = false;
    m_status = "Not supported";
    emit statusChanged();
    emit scanError("Bluetooth scanning not supported on this platform");
#endif
}

void BluetoothScanner::stopScan()
{
    if (!m_isScanning) {
        return;
    }

#ifdef Q_OS_ANDROID
    if (m_javaBluetoothScanner.isValid()) {
        m_javaBluetoothScanner.callMethod<void>("stopScan");
    }
#endif

    m_isScanning = false;
    m_status = "Stopped";
    emit statusChanged();
}

bool BluetoothScanner::checkPermissions()
{
#ifdef Q_OS_ANDROID
    qDebug() << "BluetoothScanner::checkPermissions: Starting permission check...";
    
    // 使用 QNativeInterface::QAndroidApplication::context() 获取 context
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    
    qDebug() << "BluetoothScanner::checkPermissions: Context obtained, isValid=" << context.isValid();
    
    if (!context.isValid()) {
        qDebug() << "BluetoothScanner::checkPermissions: Context is invalid, returning false";
        return false;
    }

    qDebug() << "BluetoothScanner::checkPermissions: Calling checkSelfPermission...";
    QJniObject permissionStr = QJniObject::fromString("android.permission.BLUETOOTH_SCAN");
    qDebug() << "BluetoothScanner::checkPermissions: Permission string created, isValid=" << permissionStr.isValid();
    
    if (!permissionStr.isValid()) {
        qDebug() << "BluetoothScanner::checkPermissions: Failed to create permission string";
        return false;
    }
    
    qDebug() << "BluetoothScanner::checkPermissions: Getting jstring object...";
    jstring permStr = permissionStr.object<jstring>();
    if (permStr == nullptr) {
        qDebug() << "BluetoothScanner::checkPermissions: jstring is null";
        return false;
    }
    qDebug() << "BluetoothScanner::checkPermissions: jstring obtained, calling checkSelfPermission...";
    
    QJniEnvironment env;
    // checkSelfPermission 返回 int，不是对象，所以使用 callMethod<jint>
    jint result = context.callMethod<jint>(
        "checkSelfPermission",
        "(Ljava/lang/String;)I",
        permStr
    );
    
    if (env.checkAndClearExceptions()) {
        qDebug() << "BluetoothScanner::checkPermissions: Exception calling checkSelfPermission";
        return false;
    }
    
    qDebug() << "BluetoothScanner::checkPermissions: Permission result=" << result << " (0=GRANTED)";
    
    bool granted = (result == 0); // PackageManager.PERMISSION_GRANTED = 0
    qDebug() << "BluetoothScanner::checkPermissions: Returning " << granted;
    return granted;
#else
    return true;
#endif
}

void BluetoothScanner::requestPermissions()
{
#ifdef Q_OS_ANDROID
    qDebug() << "BluetoothScanner::requestPermissions: Starting permission request...";
    
    // 尝试使用 Qt 6.2+ 的 QBluetoothPermission API（类似定位权限的自动请求）
    #if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    // 检查 QBluetoothPermission 是否可用（通过检查类是否存在）
    // 如果编译时可用，使用它；否则使用 JNI
    QBluetoothPermission bluetoothPermission;
    Qt::PermissionStatus status = qApp->checkPermission(bluetoothPermission);
    
    qDebug() << "BluetoothScanner::requestPermissions: Current permission status=" << status;
    
    if (status == Qt::PermissionStatus::Undetermined || status == Qt::PermissionStatus::Denied) {
        qDebug() << "BluetoothScanner::requestPermissions: Requesting Bluetooth permission via QBluetoothPermission...";
        qApp->requestPermission(bluetoothPermission, [this](const QPermission &permission) {
            Qt::PermissionStatus permStatus = permission.status();
            qDebug() << "BluetoothScanner::requestPermissions: Permission request result=" << permStatus;
            
            if (permStatus == Qt::PermissionStatus::Granted) {
                m_status = "Bluetooth permissions granted";
                qDebug() << "BluetoothScanner::requestPermissions: Permissions granted successfully";
                emit statusChanged();
                emit permissionRequestFinished(true);
                
                // 如果有待执行的扫描，自动继续
                if (m_pendingScanTimeout >= 0) {
                    qDebug() << "BluetoothScanner::requestPermissions: Auto-continuing scan with timeout=" << m_pendingScanTimeout;
                    int timeout = m_pendingScanTimeout;
                    m_pendingScanTimeout = -1;  // 清除待执行标志
                    // 使用 QMetaObject::invokeMethod 确保在主线程执行
                    QMetaObject::invokeMethod(this, "startScan", Qt::QueuedConnection, Q_ARG(int, timeout));
                }
            } else {
                m_status = "Bluetooth permissions denied";
                qDebug() << "BluetoothScanner::requestPermissions: Permissions denied";
                emit statusChanged();
                emit permissionRequestFinished(false);
                m_pendingScanTimeout = -1;  // 清除待执行标志
                emit scanError("Bluetooth permissions denied. Please grant permissions in settings.");
            }
        });
    } else if (status == Qt::PermissionStatus::Granted) {
        m_status = "Bluetooth permissions already granted";
        qDebug() << "BluetoothScanner::requestPermissions: Permissions already granted";
        emit statusChanged();
        emit permissionRequestFinished(true);
        
        // 如果有待执行的扫描，自动继续
        if (m_pendingScanTimeout >= 0) {
            qDebug() << "BluetoothScanner::requestPermissions: Permissions already granted, auto-continuing scan with timeout=" << m_pendingScanTimeout;
            int timeout = m_pendingScanTimeout;
            m_pendingScanTimeout = -1;  // 清除待执行标志
            QMetaObject::invokeMethod(this, "startScan", Qt::QueuedConnection, Q_ARG(int, timeout));
        }
    }
    #else
    // Qt < 6.2 或 QBluetoothPermission 不可用，使用 JNI 方式
    qDebug() << "BluetoothScanner::requestPermissions: Using JNI method (Qt < 6.2 or QBluetoothPermission not available)";
    requestPermissionsViaJNI();
    #endif
#else
    m_status = "Not supported on this platform";
    emit statusChanged();
#endif
}

#ifdef Q_OS_ANDROID
void BluetoothScanner::requestPermissionsViaJNI()
{
    qDebug() << "BluetoothScanner::requestPermissionsViaJNI: Starting...";
    
    // 获取 Activity
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;"
    );
    
    if (!activity.isValid()) {
        qDebug() << "BluetoothScanner::requestPermissionsViaJNI: Failed to get Activity";
        m_status = "Error: Failed to get Activity";
        emit statusChanged();
        return;
    }
    
    qDebug() << "BluetoothScanner::requestPermissionsViaJNI: Got Activity, requesting permissions...";
    
    // 创建权限数组
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
    
    // 调用 requestPermissions
    // 注意：这需要 ActivityResultLauncher，但为了简化，我们使用旧的方式
    // 对于 Android 12+，可能需要使用 ActivityResultLauncher
    jint requestCode = 1001;
    activity.callMethod<void>("requestPermissions",
                              "([Ljava/lang/String;I)V",
                              permissionsArray,
                              requestCode);
    
    if (env.checkAndClearExceptions()) {
        qDebug() << "BluetoothScanner::requestPermissionsViaJNI: Exception occurred";
        m_status = "Error: Failed to request permissions";
    } else {
        qDebug() << "BluetoothScanner::requestPermissionsViaJNI: Permission request sent";
        m_status = "Permission request sent, waiting for user response";
    }
    
    env->DeleteLocalRef(permissionsArray);
    env->DeleteLocalRef(stringClass);
    emit statusChanged();
}
#endif

void BluetoothScanner::onScanResult(const QString &name, const QString &address)
{
    QString deviceInfo = name.isEmpty() ? address : QString("%1 (%2)").arg(name, address);

    if (!m_deviceList.contains(deviceInfo)) {
        m_deviceList.append(deviceInfo);
        emit deviceFound(name, address);
        emit deviceListChanged();
    }
}

void BluetoothScanner::onScanFinished()
{
    m_isScanning = false;
    m_status = QString("Found %1 device(s)").arg(m_deviceList.size());
    emit statusChanged();
    emit scanFinished();
}

void BluetoothScanner::onScanError(const QString &error)
{
    m_isScanning = false;
    m_status = "Error: " + error;
    emit statusChanged();
    emit scanError(error);
}

#ifdef Q_OS_ANDROID
void BluetoothScanner::initializeJavaObject()
{
    qDebug() << "BluetoothScanner::initializeJavaObject: Starting initialization...";
    qDebug() << "BluetoothScanner::initializeJavaObject: this pointer=" << reinterpret_cast<void*>(this);
    QJniEnvironment env;

    // 获取 Android Context
    qDebug() << "BluetoothScanner::initializeJavaObject: Getting Android context...";
    QJniObject context = QNativeInterface::QAndroidApplication::context();

    if (env.checkAndClearExceptions()) {
        qDebug() << "BluetoothScanner::initializeJavaObject: Exception getting context";
        return;
    }

    if (!context.isValid()) {
        qDebug() << "BluetoothScanner::initializeJavaObject: Failed to get Android context";
        return;
    }
    qDebug() << "BluetoothScanner::initializeJavaObject: Got Android context successfully";

    qDebug() << "BluetoothScanner::initializeJavaObject: Getting context jobject...";
    // 创建 Java 对象 - 使用 context 的 jobject
    jobject contextObj = context.object();
    if (contextObj == nullptr) {
        qDebug() << "BluetoothScanner::initializeJavaObject: Context object is null";
        return;
    }
    qDebug() << "BluetoothScanner::initializeJavaObject: Context jobject obtained, ptr=" << reinterpret_cast<void*>(contextObj);

    qDebug() << "BluetoothScanner::initializeJavaObject: Creating Java BluetoothScanner object...";
    m_javaBluetoothScanner = QJniObject("org/qtproject/example/quarcs/BluetoothScanner",
                                        "(Landroid/content/Context;)V",
                                        contextObj);

    if (env.checkAndClearExceptions()) {
        qDebug() << "BluetoothScanner::initializeJavaObject: Exception creating Java object";
        m_javaBluetoothScanner = QJniObject();
        return;
    }

    if (!m_javaBluetoothScanner.isValid()) {
        qDebug() << "BluetoothScanner::initializeJavaObject: Failed to create Java BluetoothScanner object";
        return;
    }
    qDebug() << "BluetoothScanner::initializeJavaObject: Java object created successfully";
    qDebug() << "BluetoothScanner::initializeJavaObject: Java object pointer=" << reinterpret_cast<void*>(m_javaBluetoothScanner.object());

    // 连接信号和槽
    qDebug() << "BluetoothScanner::initializeJavaObject: Calling setQtObject, ptr=" << reinterpret_cast<jlong>(this);
    QJniObject::callStaticMethod<void>(
        "org/qtproject/example/quarcs/BluetoothScanner",
        "setQtObject",
        "(J)V",
        reinterpret_cast<jlong>(this)
    );

    if (env.checkAndClearExceptions()) {
        qDebug() << "BluetoothScanner::initializeJavaObject: Exception calling setQtObject";
    } else {
        qDebug() << "BluetoothScanner::initializeJavaObject: setQtObject called successfully";
    }

    // 连接 Java 回调到 C++ 槽
    qDebug() << "BluetoothScanner::initializeJavaObject: Calling setCallbacks, ptr=" << reinterpret_cast<jlong>(this);
    QJniObject::callStaticMethod<void>(
        "org/qtproject/example/quarcs/BluetoothScanner",
        "setCallbacks",
        "(J)V",
        reinterpret_cast<jlong>(this)
    );

    if (env.checkAndClearExceptions()) {
        qDebug() << "BluetoothScanner::initializeJavaObject: Exception calling setCallbacks";
    } else {
        qDebug() << "BluetoothScanner::initializeJavaObject: setCallbacks called successfully";
    }

    qDebug() << "BluetoothScanner::initializeJavaObject: Initialization completed";
}

// JNI 方法实现 - 从 Java 回调到 C++
extern "C" {
    JNIEXPORT void JNICALL
    Java_org_qtproject_example_quarcs_BluetoothScanner_onDeviceFoundNative(JNIEnv *env, jclass, jlong ptr, jstring name, jstring address)
    {
        qDebug() << "JNI: onDeviceFoundNative called, ptr=" << ptr;
        BluetoothScanner *scanner = reinterpret_cast<BluetoothScanner *>(ptr);
        if (scanner) {
            qDebug() << "JNI: Scanner pointer valid, extracting strings...";
            const char *nameStr = env->GetStringUTFChars(name, nullptr);
            const char *addressStr = env->GetStringUTFChars(address, nullptr);
            QString deviceName = QString::fromUtf8(nameStr);
            QString deviceAddress = QString::fromUtf8(addressStr);
            env->ReleaseStringUTFChars(name, nameStr);
            env->ReleaseStringUTFChars(address, addressStr);
            qDebug() << "JNI: Device found: " << deviceName << " (" << deviceAddress << ")";

            qDebug() << "JNI: Invoking onScanResult method...";
            QMetaObject::invokeMethod(scanner, "onScanResult", Qt::QueuedConnection,
                                      Q_ARG(QString, deviceName),
                                      Q_ARG(QString, deviceAddress));
            qDebug() << "JNI: onDeviceFoundNative completed";
        } else {
            qDebug() << "JNI: ERROR - Scanner pointer is null!";
        }
    }

    JNIEXPORT void JNICALL
    Java_org_qtproject_example_quarcs_BluetoothScanner_onScanFinishedNative(JNIEnv *env, jclass, jlong ptr)
    {
        Q_UNUSED(env)
        qDebug() << "JNI: onScanFinishedNative called, ptr=" << ptr;
        BluetoothScanner *scanner = reinterpret_cast<BluetoothScanner *>(ptr);
        if (scanner) {
            qDebug() << "JNI: Scanner pointer valid, invoking onScanFinished...";
            QMetaObject::invokeMethod(scanner, "onScanFinished", Qt::QueuedConnection);
            qDebug() << "JNI: onScanFinishedNative completed";
        } else {
            qDebug() << "JNI: ERROR - Scanner pointer is null!";
        }
    }

    JNIEXPORT void JNICALL
    Java_org_qtproject_example_quarcs_BluetoothScanner_onScanErrorNative(JNIEnv *env, jclass, jlong ptr, jstring error)
    {
        qDebug() << "JNI: onScanErrorNative called, ptr=" << ptr;
        BluetoothScanner *scanner = reinterpret_cast<BluetoothScanner *>(ptr);
        if (scanner) {
            qDebug() << "JNI: Scanner pointer valid, extracting error string...";
            const char *errorStr = env->GetStringUTFChars(error, nullptr);
            QString errorMsg = QString::fromUtf8(errorStr);
            env->ReleaseStringUTFChars(error, errorStr);
            qDebug() << "JNI: Error message: " << errorMsg;

            qDebug() << "JNI: Invoking onScanError method...";
            QMetaObject::invokeMethod(scanner, "onScanError", Qt::QueuedConnection,
                                      Q_ARG(QString, errorMsg));
            qDebug() << "JNI: onScanErrorNative completed";
        } else {
            qDebug() << "JNI: ERROR - Scanner pointer is null!";
        }
    }
}
#endif


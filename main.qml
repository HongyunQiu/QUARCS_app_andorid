import QtQuick //2.12
import QtQuick.Window //2.12
import QtQuick.Layouts //1.12

import QtQuick.Controls //2.12
import QtWebView //1.1
import QtWebSockets 1.5 // 导入 Qt 网络模块
import CustomTypes 1.0 // 导入 ServerFinder 类所在的命名空间
import QtQuick.LocalStorage 2.0//存储APP的设置

import Qt.labs.folderlistmodel //2.0
import Qt.labs.platform

import Qt.labs.settings 6.0 //Qt.labs.settings 1.0
import QtPositioning 6.5

ApplicationWindow {
    id: appWindow
    visible: true
    width: Screen.width
    height: Screen.height
    title: qsTr("QUARCS")
    visibility:"FullScreen"//Window.FullScreen兼容QT6  //全屏显示
    flags: Qt.Window | Qt.FramelessWindowHint  //| Qt.MaximizeUsingFullscreenGeometryHint// 设置为无边框窗口
    
    // 定义信号用于添加命令日志
    signal commandLogRequested(string cmd, string type)

    Image {
        id:bground
        anchors.fill: parent // 图像填充整个窗口
        source: "qrc:/img/Image_BG.png"
        fillMode: Image.Stretch // 拉伸模式
        //opacity: 1.0 // 设置透明度为 1，确保图片完全显示
    }

    // 创建 ServerFinder 对象
    property var serverFinder: ServerFinder{}
    // 创建 WifiInfoProvider 对象
    property var wifiInfoProvider: WifiInfoProvider{}
    // 创建 BluetoothScanner 对象
    // 使用 QtBluetoothScanner（基于 Qt Bluetooth 模块，跨平台支持）
    // 如需使用原有的 JNI 实现，可改为: property var bluetoothScanner: BluetoothScanner{}
    property var bluetoothScanner: QtBluetoothScanner{}
    property bool privacyAccepted: false
    
    // 处理命令日志请求的信号处理器
    onCommandLogRequested: function(cmd, type) {
        if (typeof commandLogModel === 'undefined') {
            return;
        }
        
        var now = new Date();
        var timestamp = String(now.getHours()).padStart(2, '0') + ":" + 
                       String(now.getMinutes()).padStart(2, '0') + ":" + 
                       String(now.getSeconds()).padStart(2, '0');
        
        commandLogModel.append({
            "timestamp": timestamp,
            "command": cmd,
            "type": type  // "sent" 或 "received"
        });
        
        // 限制日志条数（最多保留100条）
        if (commandLogModel.count > 100) {
            commandLogModel.remove(0);
        }
    }
    
    // 添加命令日志的辅助函数（ApplicationWindow 根级别）
    function addCommandLog(cmd, type) {
        commandLogRequested(cmd, type);
    }
    function syncEmbeddedRuntimeState() {
        if (typeof appHostService === "undefined" || !appHostService) {
            return;
        }
        appHostService.updateRuntimeState(
            currentTime,
            currentLat,
            currentLong,
            currentLanguage,
            wifiInfoProvider.wifiInfo,
            appVersion
        );
    }
    function openEmbeddedFrontend() {
        if (!embeddedFrontendModeEnabled || !embeddedFrontendEntryUrl) {
            return;
        }
        syncEmbeddedRuntimeState();
        elapsedTime = 0;
        webFailRectangle.visible = false;
        webViewURL.hasErrorOccurred = false;
        webDialog.visible = true;
        scanDialog.visible = false;
        startDialog.visible = false;
        bgroundText.text = currentLanguage === "zh" ? qsTr("正在加载中···") : qsTr("Loading...");
        webViewURL.url = embeddedFrontendEntryUrl;
    }
    // 扫描完成标志
    property bool wifiScanCompleted: false
    property bool bluetoothScanCompleted: false
    // 添加语言切换功能变量，默认英文
    property string currentLanguage: "en"
    property bool isInitialized: false  // 标志，用于控制初始化时不更新数据库
    Settings {
        id: languageSettings
        property string language: "en" // 默认语言
    }
    // 用于跟踪更多的当前页面
    property string currentPage: "setmain"; // 默认更多页面显示主页面-
    // 选中的 IP 地址
    property string selectedIp: "";
    // 在 QML 文件的顶部定义全局变量
    property var ipList: [] //var   全局变量，用于存储 IP 地址列表
    property var ipVersions: [] // 与 ipList 对应的版本号列表
    // 定义计时器和变量
    property int elapsedTime: 0  // 用于存储经过的时间（秒）
    // 使用 Qt.labs.folderlistmodel 来读取文件
    property string fileContent: ""
    property real aspectRatio: Screen.width / Screen.height // 定义宽高比变量
    // 添加模式功能变量，默认WIFI模式说明
    //property string currentMode: "wifi"
    property string currentTime: ""// 定义一个属性来存储当前时间
    property string currentLat: "0"//纬度
    property string currentLong: "0"//经度
    property string currentAlt: "0"//海拔
    property bool embeddedFrontendModeEnabled: typeof embeddedFrontendEnabled !== "undefined" ? embeddedFrontendEnabled : false
    property url embeddedFrontendEntryUrl: typeof embeddedFrontendUrl !== "undefined" ? embeddedFrontendUrl : ""
    property bool embeddedFrontendAutoStart: typeof embeddedFrontendAutoLaunch !== "undefined" ? embeddedFrontendAutoLaunch : false
    property var majorVersions: []//依据大版本号的最新版本号数组
    property url versionListUrl: "https://www.qhyccd.cn/file/repository/latestSoftAndDirver/QUARCS/QUARCSversions.txt"
    property bool hasFetchedVersions: false//是否获取网页有效版本号数组
    property string versionWEBValue:""//
    property var deviceHasLocalPackages: [] // 对应 ipList 的本地更新包标记
    property var deviceLocalPaths: [] // 与 deviceHasLocalPackages 对应的本地文件路径
    property var deviceUploadStatusMap: ({})

    onCurrentTimeChanged: syncEmbeddedRuntimeState()
    onCurrentLatChanged: syncEmbeddedRuntimeState()
    onCurrentLongChanged: syncEmbeddedRuntimeState()
    onCurrentLanguageChanged: syncEmbeddedRuntimeState()

    onIpListChanged: {
        var newMap = ({})
        if (ipList && ipList.length) {
            for (var i = 0; i < ipList.length; i++) {
                var ip = ipList[i]
                if (deviceUploadStatusMap && deviceUploadStatusMap[ip] === true) {
                    newMap[ip] = true
                } else {
                    newMap[ip] = false
                }
            }
        }
        deviceUploadStatusMap = newMap
        checkLocalUpdatePackages()
    }
    onIpVersionsChanged: checkLocalUpdatePackages()
    onMajorVersionsChanged: {
        if (hasFetchedVersions)
            checkLocalUpdatePackages()
    }
    onHasFetchedVersionsChanged: {
        if (hasFetchedVersions)
            checkLocalUpdatePackages()
        else {
            deviceHasLocalPackages = []
            deviceLocalPaths = []
        }
    }

//    // 位置源      QGeoPositionInfoSource
    PositionSource{
        id: positionSource
        active:false//true //false // 启用定位
        // updateInterval: 60000  // ms每1秒更新一次
        preferredPositioningMethods: PositionSource.AllPositioningMethods//SatellitePositioningMethods // 使用卫星定位以获取海拔
        onPositionChanged: {
            let coord = position.coordinate
            // console.log("定位方法:", positionSource.positioningMethod)
            // if (positionSource.positioningMethod === PositionSource.NonSatellitePositioningMethod) {
            //         console.log("❌ 使用基站/WiFi定位，不支持海拔测量")
            //     }
            // console.log("原始海拔值:", coord.altitude)
            // console.log("海拔数据类型:", typeof coord.altitude)
            // console.log("海拔原始字符串:", coord.altitude.toString())
            // console.log("海拔垂直精度:", position.verticalAccuracy)
            // console.log("JSON格式:", JSON.stringify({
            //                                           latitude: coord.latitude,
            //                                           longitude: coord.longitude,
            //                                           altitude: coord.altitude,
            //                                           altitudeType: typeof coord.altitude,
            //                                           isValid: !isNaN(coord.altitude) && isFinite(coord.altitude)
            //                                       }))

            currentLat=coord.latitude.toFixed(6)
            currentLong=coord.longitude.toFixed(6)
            // currentAlt=coord.altitude.toFixed(2)
            console.log("定位信息获取Lat.:", coord.latitude, "Long.:", coord.longitude)
            locationLat.text = currentLat === 0?(currentLanguage === "zh" ? qsTr("纬度:未获取") : qsTr("Latitude: Not acquired")):(currentLanguage === "zh" ? qsTr("纬度:"+currentLat): qsTr("Latitude: "+currentLat))
            locationLong.text = currentLong === 0?(currentLanguage === "zh" ? qsTr("经度:未获取") : qsTr("Longitude: Not acquired")):(currentLanguage === "zh" ? qsTr("经度:"+currentLong): qsTr("Longitude: "+currentLong))
            // locationAlt.text = currentAlt === 0?(currentLanguage === "zh" ? qsTr("海拔:未获取") : qsTr("Altitude: Not acquired")):(currentLanguage === "zh" ? qsTr("海拔:"+currentAlt): qsTr("Altitude: "+currentAlt))
            positionSource.active = false  // 关闭持续更新，只获取一次
        }
        onSourceErrorChanged: {
            if (sourceError === PositionSource.AccessError) {
                console.log("定位权限被拒绝")
                locationLat.text = currentLanguage === "zh" ? qsTr("纬度：无权限") : qsTr("Latitude: No permission");
                locationLong.text = currentLanguage === "zh" ? qsTr("经度：无权限") : qsTr("Longitude: No permission");
                // locationAlt.text = currentLanguage === "zh" ? qsTr("海拔：无权限") : qsTr("Altitude: No permission")
            }
        }
    }

    Component.onCompleted: {
        //获取定位权限
        console.log("Component.onCompleted aspectRatio : ", aspectRatio.toFixed(2),"=",Screen.width,"/",Screen.height)
        positionSource.active = true
        updateTime()
        wifiInfoProvider.updateWifiInfo()
        // 在启动时加载保存的语言设置
        if (languageSettings.language !== "") {
            currentLanguage = languageSettings.language
        }
        if (0)//0 Qt.platform.os !== "android" && Qt.platform.os !== "ios")
        {
            var db = LocalStorage.openDatabaseSync("AppData", "1.0", "Local storage for app", 100);
            db.transaction(function(tx) {
                tx.executeSql('CREATE TABLE IF NOT EXISTS Settings(key TEXT, value TEXT)');
                var rs = tx.executeSql('SELECT value FROM Settings WHERE key = "privacyAccepted"');
                if (rs.rows.length > 0) {
                    privacyAccepted = rs.rows.item(0).value === "true";
                }
                else {
                    privacyAccepted = false;
                }
            });
            if (!privacyAccepted) {
                privacyPolicyDialog.visible = true;
            }
            else {
                privacyPolicyDialog.visible = false;
                startDialog.visible=true;
            }
        }
        else {
            privacyPolicyDialog.visible = false;//隐私政策显示隐藏
            ipRectangle.visible=false;//IP输入提示隐藏
            webDialog.visible=false;//Web页面设置隐藏
            webViewURL.visible = false;//WebView设置隐藏
            moreDialog.visible=false;//更多页面 隐藏
            locationDialog.visible=false;//经纬度信息页面隐藏
            scanDialog.visible=false;//扫描页面 隐藏
            bluetoothControlDialog.visible=false;//蓝牙控制页面 隐藏
            startDialog.visible=true;
            syncEmbeddedRuntimeState();
            if (embeddedFrontendAutoStart) {
                Qt.callLater(function() {
                    openEmbeddedFrontend();
                });
            }
        }
    }

    Timer {
        id: timer
        interval: 1000  // 每秒触发一次
        running: false  // 初始时计时器不运行
        repeat: true  // 使计时器不断重复
        onRunningChanged: {
            console.log("Timer running state changed: " + running);
        }
        onTriggered: {
            elapsedTime += 1  // 每次触发时增加 1 秒
            console.log("Timer triggered: elapsedTime = " + elapsedTime + ", startDialog.visible = " + startDialog.visible + ", startDialog.enabled = " + startDialog.enabled);
            // 更新 startText：当 startDialog 可见且未启用时（正在扫描中）
            if(startDialog.visible && startDialog.enabled===false){
                startText.text=currentLanguage === "zh" ?qsTr("开启中···"+elapsedTime ):qsTr("Starting... "+elapsedTime )
            }
            if(btn_scan.enabled===false){
                btn_scanText.text=currentLanguage === "zh" ?qsTr(""+elapsedTime ):qsTr(""+elapsedTime ) // 更新扫描按钮文本，显示经过的秒数
            }
            if(stackLayout_more.currentIndex === moreDialog.updatePageIndex){//updateRectangle.visible
                btn_update_scanText.text=currentLanguage === "zh" ?qsTr(""+elapsedTime ):qsTr(""+elapsedTime )
            }
            if(webDialog.visible===true  && webViewURL.visible === false){//&& webDialog.enabled===true
                // if(elapsedTime>70 && webViewURL.loadProgress<70){elapsedTime=70}
                bgroundText.text=currentLanguage === "zh" ? qsTr("正在加载中···"+elapsedTime +"秒 ("+webViewURL.loadProgress+"%)"): qsTr("Loading... "+elapsedTime +"s ("+webViewURL.loadProgress+"%)")
                //bgText.text=qsTr("stop"+webDialog.isStopped)
            }
        }
    }
    // 时间更新Timer - 控制时间显示更新
    Timer {
        id: timeUpdateTimer
        interval: 1000
        repeat: true
        running: true
        onTriggered:{
            updateTime()
        }
    }

    //下载更新包成功后显示计时3s
    Timer {
            id: updateDialogAutoCloseTimer
            interval: 3000
            repeat: false
            onTriggered: {
                if (updateDialog.visible) {
                    updateDialog.visible = false
                    scanDialog.visible = true
                }
                if(updateDialog.mode==="download")
                {
                    btn_scan.clicked()//避免下载更新包后列表不更新问题
                }
            }
        }
    //隐私政策
    PrivacyPolicyDialog {
        id: privacyPolicyDialog
        visible: false
        anchors.centerIn: parent
        currentLanguage: currentLanguage
        onAccepted: {
            var db = LocalStorage.openDatabaseSync("AppData", "1.0", "Local storage for app", 100);
            db.transaction(function(tx) {
                tx.executeSql('INSERT OR REPLACE INTO Settings VALUES (?, ?)', ["privacyAccepted", "true"]);
            });
            privacyPolicyDialog.visible = false;
        }
        onRejected: {
            Qt.quit();
        }
    }

    Connections {
        target: wifiInfoProvider
        function onWifiInfoChanged(info)
        {
            console.log("wifiInfoProvider onWifiInfoChanged is:", info,hasFetchedVersions,majorVersions)
            wifitext.text="WiFi: "+info
            syncEmbeddedRuntimeState()
            //获取网页的盒子各大版本号的最新版本号
            if (!hasFetchedVersions || majorVersions.length === 0)
            {
                fetchMajorVersions()
                // hasFetchedVersions=true
                // majorVersions=["1.9.4","2.6.4","3.8.7"]
                console.log("fetchMajorVersions  is:", majorVersions)
            }
            if(scanDialog.visible)
            {
                btn_scan.clicked();
            }
        }
    }
    
    // 监听 BluetoothScanner 的信号（ApplicationWindow 级别）
    Connections {
        target: bluetoothScanner
        function onDeviceFound(name, address) {
            console.log("Bluetooth device found: " + name + " (" + address + ")");
            
            // 判断是否是ESP32-Stepper设备
            if (name.indexOf("ESP32-Stepper") >= 0) {
                console.log("ESP32-Stepper device detected: " + name + " (" + address + ")");
                // 将ESP32-Stepper设备的MAC地址添加到ipList中
                // 格式：设备名称 (MAC地址)，例如："ESP32-Stepper (28:39:26:9C:E3:FE)"
                var deviceInfo = name + " (" + address + ")";
                
                // 检查是否已存在，避免重复添加
                var exists = false;
                for (var i = 0; i < ipList.length; i++) {
                    if (ipList[i] === deviceInfo || ipList[i] === address) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    // 添加到ipList
                    var newList = ipList.slice(); // 创建副本
                    newList.push(deviceInfo);
                    ipList = newList;
                    console.log("ESP32-Stepper device added to ipList: " + deviceInfo);
                    console.log("Current ipList length: " + ipList.length);
                } else {
                    console.log("ESP32-Stepper device already in ipList: " + deviceInfo);
                }
            }
        }
        function onScanFinished() {
            var bluetoothScanTime = elapsedTime;  // 保存蓝牙扫描完成时的时间
            console.log("Bluetooth scan finished at " + bluetoothScanTime + " seconds. Found " + bluetoothScanner.deviceList.length + " device(s)");
            console.log("Total scan time (WiFi + Bluetooth): " + bluetoothScanTime + " seconds");
            bluetoothScanCompleted = true
            
            if (wifiScanCompleted && bluetoothScanCompleted) {
                console.log("Both scans completed at " + elapsedTime + " seconds, stopping timer and resetting");
                
                // 根据当前可见的对话框处理
                if (startDialog.visible) {
                    // 主页面扫描完成
                    startDialog.visible=false;
                    scanDialog.visible=true;
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    updateIPlistshow_color();
                } else if (scanDialog.visible) {
                    // 扫描页面扫描完成
                    // 更新控件显示
                    btn_scanText.text = currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan");
                    btn_scan.enabled = true;
                    btn_Img_scan.opacity=1;
                    btn_scan_back.enabled=true;
                    listContent.enabled=true;
                    btn_Img_scan_back.opacity=1;
                    vdVirDevice.enabled=true
                    btn_Img_scan_vd.opacity=1
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    selectedIp=""
                    updateIPlistshow_color();
                }
            } else {
                console.log("Bluetooth scan done, but WiFi scan not completed yet. Timer continues at " + elapsedTime);
            }
        }
        function onScanError(error) {
            console.log("Bluetooth scan error: " + error);
            bluetoothScanCompleted = true  // 即使出错也标记为完成，继续执行
            
            if (wifiScanCompleted && bluetoothScanCompleted) {
                console.log("Both scans completed (with error) at " + elapsedTime + " seconds, stopping timer and resetting");
                
                // 根据当前可见的对话框处理
                if (startDialog.visible) {
                    // 主页面扫描完成
                    startDialog.visible=false;
                    scanDialog.visible=true;
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    updateIPlistshow_color();
                } else if (scanDialog.visible) {
                    // 扫描页面扫描完成
                    // 更新控件显示
                    btn_scanText.text = currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan");
                    btn_scan.enabled = true;
                    btn_Img_scan.opacity=1;
                    btn_scan_back.enabled=true;
                    listContent.enabled=true;
                    btn_Img_scan_back.opacity=1;
                    vdVirDevice.enabled=true
                    btn_Img_scan_vd.opacity=1
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    selectedIp=""
                    updateIPlistshow_color();
                }
            } else {
                console.log("Bluetooth scan error, but WiFi scan not completed yet. Timer continues at " + elapsedTime);
            }
        }
        
        // 监听蓝牙连接状态
        function onConnectionStateChanged(connected) {
            if (connected) {
                console.log("Bluetooth device connected: " + bluetoothScanner.connectedDeviceAddress);
                // showTemporaryTip(currentLanguage === "zh" ?
                //     qsTr("蓝牙设备已连接") :
                //     qsTr("Bluetooth device connected"));
            } else {
                console.log("Bluetooth device disconnected");
                showTemporaryTip(currentLanguage === "zh" ? 
                    qsTr("蓝牙设备已断开") : 
                    qsTr("Bluetooth device disconnected"));
            }
        }
        
        function onServiceDiscovered() {
            console.log("Bluetooth service discovered");
        }
        
        function onCharacteristicReady() {
            console.log("Bluetooth characteristic ready, device is ready for commands");
            // showTemporaryTip(currentLanguage === "zh" ?
            //     qsTr("蓝牙设备已就绪，可以发送命令") :
            //     qsTr("Bluetooth device ready, commands can be sent"));
            // 打开蓝牙控制页面
            // 隐藏其他页面
            scanDialog.visible = false;
            startDialog.visible = false;
            webDialog.visible = false;
            moreDialog.visible = false;
            // 显示蓝牙控制页面
            bluetoothControlDialog.visible = true;
        }
        
        function onDataReceived(data) {
            console.log("Bluetooth data received: " + data);
            
            // 转换 QByteArray 到字符串
            var message = data.toString();
            
            // 触发接收动画
            if (bluetoothControlDialog.visible && typeof receiveAnimation !== 'undefined') {
                receiveAnimation.start();
            }
            
            // 添加到命令日志
            appWindow.addCommandLog(message, "received");
            
            // 解析响应消息（根据协议定义）
            if (message.startsWith("OK:")) {
                // 命令执行成功
                var confirmedCmd = message.substring(3);
                console.log("Command confirmed: " + confirmedCmd);
                
                // 检查是否是停止命令的确认
                if (confirmedCmd.indexOf("stop") >= 0) {
                    console.log("Stop command confirmed: " + confirmedCmd);
                    // 如果正在进行紧急停止，可以在这里更新状态
                    if (bluetoothControlDialog.emergencyStopInProgress) {
                        showTemporaryTip(currentLanguage === "zh" ? 
                            "停止指令已确认" : 
                            "Stop command confirmed");
                    }
                } else {
                    showTemporaryTip(currentLanguage === "zh" ? 
                        "指令已确认: " + confirmedCmd : 
                        "Command confirmed: " + confirmedCmd);
                }
            } else if (message.startsWith("ERROR:")) {
                // 命令执行失败
                var errorMsg = message.substring(6);
                console.log("Command error: " + errorMsg);
                showTemporaryTip(currentLanguage === "zh" ? 
                    "指令错误: " + errorMsg : 
                    "Command error: " + errorMsg);
            } else if (message.startsWith("STATUS:")) {
                // 状态消息
                var statusMsg = message.substring(7);
                console.log("Device status: " + statusMsg);
            } else {
                // 其他消息
                console.log("Device message: " + message);
            }
        }
        
        function onConnectionError(error) {
            console.log("Bluetooth connection error: " + error);
            // showTemporaryTip(currentLanguage === "zh" ?
            //     qsTr("蓝牙连接错误: " + error) :
            //     qsTr("Bluetooth connection error: " + error));
            // 隐藏其他页面
            scanDialog.visible = false;
            startDialog.visible = false;
            webDialog.visible = false;
            moreDialog.visible = false;
            // 显示蓝牙控制页面
            bluetoothControlDialog.visible = true;
        }
    }
    
    // 监听 serverFinder 的信号（ApplicationWindow 级别）
    Connections {
        target: serverFinder
        function onServerAddressesFound(addresses, versions){
            // 根据当前可见的对话框处理不同的扫描场景
            if (startDialog.visible) {
                // 初始扫描（从startDialog触发）
                console.log("onServerAddressesFound: startDialog visible, processing initial scan");
                
                wifiScanCompleted = true
                ipList = addresses;
                ipVersions = versions || [];
                
                var wifiScanTime = elapsedTime;  // 保存WiFi扫描完成时的时间
                console.log("WiFi scan completed at " + wifiScanTime + " seconds");
                console.log("Timer running: " + timer.running + ", elapsedTime: " + elapsedTime);
                
                // WiFi 扫描完成后，开始蓝牙扫描（不重置elapsedTime，继续计时）
                if (Qt.platform.os === "android" || Qt.platform.os === "ios") {
                    console.log("WiFi scan completed, starting Bluetooth scan...");
                    console.log("Timer will continue from " + elapsedTime + " seconds");
                    
                    // // 添加测试蓝牙设备（用于测试）
                    // var testDeviceInfo = "ESP32-Stepper-AAAA (AA:BB:CC:DD:EE:FF)";
                    // var testList = ipList.slice();
                    // // 检查是否已存在，避免重复添加
                    // var testExists = false;
                    // for (var i = 0; i < testList.length; i++) {
                    //     if (testList[i] === testDeviceInfo || testList[i].indexOf("AA:BB:CC:DD:EE:FF") >= 0) {
                    //         testExists = true;
                    //         break;
                    //     }
                    // }
                    // if (!testExists) {
                    //     testList.push(testDeviceInfo);
                    //     ipList = testList;
                    //     console.log("Test Bluetooth device added: " + testDeviceInfo);
                    // }
                    
                    bluetoothScanner.startScan(10000);  // 扫描 10 秒
                    // 确保计时器继续运行（不重置elapsedTime）
                    if (!timer.running) {
                        console.log("Timer was stopped, restarting it (elapsedTime preserved: " + elapsedTime + ")");
                        timer.start();
                    } else {
                        console.log("Timer is running, elapsedTime will continue from " + elapsedTime);
                    }
                } else {
                    // 非 Android/iOS 平台，直接标记蓝牙扫描完成
                    bluetoothScanCompleted = true
                }
                
                // 检查是否可以继续（只有在两个扫描都完成时才停止计时器）
                if (wifiScanCompleted && bluetoothScanCompleted) {
                    console.log("Both scans completed at " + elapsedTime + " seconds, stopping timer");
                    startDialog.visible=false;
                    scanDialog.visible=true;
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    updateIPlistshow_color();
                } else {
                    console.log("WiFi scan done, waiting for Bluetooth scan. Timer continues from " + elapsedTime);
                }
            } else if (scanDialog.visible) {
                // 手动扫描（从scanDialog触发）
                console.log("onServerAddressesFound: scanDialog visible, processing manual scan");
                
                // 参照主页面的逻辑：WiFi扫描完成后，不重置elapsedTime，继续计时
                wifiScanCompleted = true
                ipList = addresses;
                ipVersions = versions || [];
                
                //var wifiScanTime = elapsedTime;  // 保存WiFi扫描完成时的时间
                console.log("WiFi scan completed at " + wifiScanTime + " seconds (manual scan)");
                console.log("Timer running: " + timer.running + ", elapsedTime: " + elapsedTime);
                
                // WiFi 扫描完成后，开始蓝牙扫描（不重置elapsedTime，继续计时）
                if (Qt.platform.os === "android" || Qt.platform.os === "ios") {
                    console.log("WiFi scan completed, starting Bluetooth scan... (manual scan)");
                    console.log("Timer will continue from " + elapsedTime + " seconds");
                    
                    // // 添加测试蓝牙设备（用于测试）
                    // var testDeviceInfo = "ESP32-Stepper-AAAA (AA:BB:CC:DD:EE:FF)";
                    // var testList = ipList.slice();
                    // // 检查是否已存在，避免重复添加
                    // var testExists = false;
                    // for (var i = 0; i < testList.length; i++) {
                    //     if (testList[i] === testDeviceInfo || testList[i].indexOf("AA:BB:CC:DD:EE:FF") >= 0) {
                    //         testExists = true;
                    //         break;
                    //     }
                    // }
                    // if (!testExists) {
                    //     testList.push(testDeviceInfo);
                    //     ipList = testList;
                    //     console.log("Test Bluetooth device added: " + testDeviceInfo);
                    // }
                    
                    bluetoothScanner.startScan(10000);  // 扫描 10 秒
                    // 确保计时器继续运行（不重置elapsedTime）
                    if (!timer.running) {
                        console.log("Timer was stopped, restarting it (elapsedTime preserved: " + elapsedTime + ")");
                        timer.start();
                    } else {
                        console.log("Timer is running, elapsedTime will continue from " + elapsedTime);
                    }
                } else {
                    // 非 Android/iOS 平台，直接标记蓝牙扫描完成
                    bluetoothScanCompleted = true
                }
                
                // 检查是否可以继续（只有在两个扫描都完成时才停止计时器）
                if (wifiScanCompleted && bluetoothScanCompleted) {
                    console.log("Both scans completed at " + elapsedTime + " seconds, stopping timer (manual scan)");
                    // 更新控件显示
                    btn_scanText.text = currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan");
                    btn_scan.enabled = true;
                    btn_Img_scan.opacity=1;
                    btn_scan_back.enabled=true;
                    listContent.enabled=true;
                    btn_Img_scan_back.opacity=1;
                    vdVirDevice.enabled=true
                    btn_Img_scan_vd.opacity=1
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    selectedIp=""
                    updateIPlistshow_color();
                } else {
                    console.log("WiFi scan done, waiting for Bluetooth scan. Timer continues from " + elapsedTime + " (manual scan)");
                    // WiFi扫描完成但蓝牙扫描未完成时，先更新部分控件
                    // 保持按钮禁用状态，等待蓝牙扫描完成
                }
            } else {
                // 两个对话框都不可见时，忽略信号
                console.log("onServerAddressesFound: neither startDialog nor scanDialog visible, ignoring");
            }
        }
        function onErrorOccurred(error) {
            // 根据当前可见的对话框处理不同的扫描场景
            if (startDialog.visible) {
                // 初始扫描错误处理
                wifiScanCompleted = true  // 即使出错也标记为完成
                // 非 Android/iOS 平台或出错时，标记蓝牙扫描完成
                if (Qt.platform.os !== "android" && Qt.platform.os !== "ios") {
                    bluetoothScanCompleted = true
                }
                // 检查是否可以继续
                if (wifiScanCompleted && bluetoothScanCompleted) {
                    startDialog.visible=false;
                    scanDialog.visible=true;
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    updateIPlistshow_color();
                }
            } else if (scanDialog.visible) {
                // 手动扫描错误处理
                // 参照主页面的逻辑：即使出错也标记为完成，但继续计时直到蓝牙扫描完成
                wifiScanCompleted = true  // 即使出错也标记为完成
                
                // 非 Android/iOS 平台或出错时，标记蓝牙扫描完成
                if (Qt.platform.os !== "android" && Qt.platform.os !== "ios") {
                    bluetoothScanCompleted = true
                }
                
                // 检查是否可以继续（只有在两个扫描都完成时才停止计时器）
                if (wifiScanCompleted && bluetoothScanCompleted) {
                    console.log("Both scans completed (with error) at " + elapsedTime + " seconds, stopping timer (manual scan)");
                    // 更新控件显示
                    btn_scanText.text = currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan");
                    btn_scan.enabled = true;
                    btn_Img_scan.opacity=1;
                    btn_scan_back.enabled=true;
                    listContent.enabled=true;
                    btn_Img_scan_back.opacity=1;
                    vdVirDevice.enabled=true
                    btn_Img_scan_vd.opacity=1
                    timer.stop();  // 停止计时器
                    elapsedTime = 0;  // 重置计时
                    ipList = [];
                    ipVersions = [];
                    selectedIp=""
                    updateIPlistshow_color();
                    listContent.text= currentLanguage === "zh" ? qsTr("扫描错误"):qsTr("Scan Error")
                } else {
                    console.log("WiFi scan error, waiting for Bluetooth scan. Timer continues at " + elapsedTime + " (manual scan)");
                    // WiFi扫描出错但蓝牙扫描未完成时，保持按钮禁用状态，等待蓝牙扫描完成
                }
            } else {
                // 两个对话框都不可见时，忽略信号
                console.log("onErrorOccurred: neither startDialog nor scanDialog visible, ignoring");
            }
        }
    }
    
    //主页面
    Rectangle {
        id: startDialog
        anchors.fill: parent
        color:  "transparent"
        // Logo 图标
        Rectangle {
            id: logo
            width: aspectRatio>1.7?parent.width*0.37:parent.width*0.37
            height: aspectRatio>1.7?parent.height*0.14:parent.height*0.14
            anchors.centerIn: parent
            color:"transparent"
            Image {
                id:bc_logo
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/Image_BG_logo.png" //bc_logofillMode: Image.Stretch // 拉伸模式
                fillMode: Image.PreserveAspectFit
            }
        }
        //取消开始按钮，点击任意位置进入
        MouseArea {
            width: parent.width
            height: parent.height*0.8
            x: 0
            y:parent.height*0.2
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (embeddedFrontendModeEnabled) {
                    openEmbeddedFrontend();
                    return;
                }
                elapsedTime = 0;  // 重置计时
                console.log("Starting scan: elapsedTime reset to 0, starting timer");
                timer.start();  // 启动计时器
                console.log("Timer started, running: " + timer.running);
                // 重置扫描完成标志
                wifiScanCompleted = false
                bluetoothScanCompleted = false
                // 启动 WiFi 扫描
                serverFinder.findServerAddress(3000);  // 设置超时时间为 3000 毫秒
                startDialog.enabled=false;                    //start.visible=true;
                startText.text=currentLanguage === "zh" ?qsTr("开启中···"+0 ):qsTr("Starting... "+0)
            }
        }
        //退出按钮
        Button {
            id:btn_quit
            width: aspectRatio>1.7 ?parent.width*100/1334:parent.width*80/1334 //parent.width*100/1334
            height:btn_quit.width*9/10//parent.height*90/750
            anchors {
                top: parent.top
                right: parent.right
            }
            background: Rectangle {
                color: "transparent" // 背景色设置为透明
            }
            Image {
                source: "qrc:/img/Btn_quit.png";
                fillMode: Image.PreserveAspectFit
                width: btn_quit.width *60/100
                height: btn_quit.width *60/100
                anchors {
                    left: parent.left // 图标靠左
                    bottom: parent.bottom // 图标靠下
                    margins: 5 // 适当设置边距，使图标不紧贴按钮边缘
                }
            }
            onClicked: {
                if(Qt.platform.os==="android"){
                    permissionRequester.exitApp();  // 调用 C++ 函数退出应用
                }
                Qt.quit()  // 退出应用
            }
        }
       //定位按钮/**/ //还未实现具体功能，暂时隐藏
        Button {
            id:btn_location
            width:btn_quit.width//parent.width*100/1334
            height:btn_location.width*9/10
            anchors {
                top: parent.top
                left: btn_set.right//parent.left
            }
            background: Rectangle {
                color: "transparent" // 背景色设置为透明
            }
            Image {
                source: "qrc:/img/Btn_location.png";
                fillMode: Image.PreserveAspectFit
                width: btn_quit.width *60/100
                height: btn_quit.width *60/100
                anchors {
                    right: parent.right // 图标靠右
                    bottom: parent.bottom // 图标靠下
                    margins: 5 // 适当设置边距，使图标不紧贴按钮边缘
                }
            }
            onClicked: {
                startDialog.visible=false;
                // scanDialog.visible=false;
                locationDialog.visible=true;
            }
        }
        //设置按钮
        Button {
            id:btn_set
            width: btn_quit.width//parent.width*100/1334
            height:btn_set.width*9/10
            anchors {
                top: parent.top
                left: parent.left//btn_location.right
            }
            background: Rectangle {
                color: "transparent" // 背景色设置为透明
            }
            Image {
                source: "qrc:/img/Btn_set.png";
                fillMode: Image.PreserveAspectFit
                width: btn_quit.width *60/100
                height: btn_quit.width *60/100
                anchors {
                    right: parent.right // 图标靠右
                    bottom: parent.bottom // 图标靠下
                    margins: 5 // 适当设置边距，使图标不紧贴按钮边缘
                }
            }
            onClicked: {
                startDialog.visible=false;
                scanDialog.visible=false;
                moreDialog.visible=true;
            }
        }
        Text{
            id:startText
            font.family: (Qt.platform.os === "ios") ?"PingFang SC":"Roboto"//iOS默认中文字体
            font.pixelSize: aspectRatio>1.7 ?12:18//确保不设置时各平台字体大小一致
            anchors {//anchors.centerIn: parent
                bottom: parent.bottom
                bottomMargin: parent.height * 0.1 // 距离底部的比例边距
                horizontalCenter: parent.horizontalCenter
            }
            text: currentLanguage === "zh" ?qsTr("点击任意位置继续"):qsTr("Click anywhere to continue") //...初始文本
            color: "white"//+Screen.width+"/"+Screen.height+"="+aspectRatio.toFixed(2)   +Screen.width+"/"+Screen.height+"="+aspectRatio.toFixed(2)
        }
        
        // 设置当 startDialog 从不可见变为可见时重置按钮
        onVisibleChanged: {
            if (visible) {
                startDialog.enabled=true;
                startText.text=currentLanguage === "zh" ?qsTr("点击任意位置继续"):qsTr("Click anywhere to continue")//+aspectRatio+aspectRatio
            }
        }
    }

    //扫描页面
    Rectangle {
        id: scanDialog
        visible: false
        anchors.fill: parent
        color:  "transparent"
        //标题栏
        Rectangle {//1334*70  (0,0)
            id: title
            width:parent.width
            height:aspectRatio>1.7?parent.height*7/75:parent.height*5/75
            x:0
            y:0
            color:  "transparent"
            Image {
                id:scan_title
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/Scan_title.png" //
                opacity:1
            }
            //返回按钮
            Button{//17*30   (30,20)
                id:btn_scan_back
                width: parent.width*47/1334
                height: parent.height
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                }
                background: Rectangle {
                    color: "transparent" // 背景色设置为透明
                }
                Image {
                    id: btn_Img_scan_back
                    source: "qrc:/img/Btn_back.png"
                    width: title.width*0.013//30 // 图标的大小
                    height: title.height*3/7//30
                    anchors {
                        right: parent.right // 图标靠右
                        verticalCenter: parent.verticalCenter // 图标垂直居中//bottom: parent.bottom // 图标靠下
                        margins: 5 // 适当设置边距，使图标不紧贴按钮边缘
                    }
                }
                onClicked: {
                    scanDialog.visible=false;
                    startDialog.visible=true;
                }
            }
            //标题文字
            Text {  //anchors.verticalCenter: parent.verticalCenter //垂直居中
                anchors.centerIn: parent//anchors.fill: parent     //x: parent.height*0.7
                text:currentLanguage === "zh" ? qsTr("QUARCS服务器连接"): qsTr("QUARCS Server Connection")
                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"                
                color: "white"
                font.pixelSize: aspectRatio>1.7 ?16:24//16
            }
        }
        //内容
        Rectangle {//1274*620  (30,100)
            id: content
            width:parent.width*0.955
            height:aspectRatio>1.7?parent.height*62/75:parent.height*66/75//parent.height*0.826
            y:aspectRatio>1.7?parent.height*10/75:parent.height*7/75//parent.height*2/15
            anchors.horizontalCenter: parent.horizontalCenter//水平居中            //anchors.verticalCenter: parent.verticalCenter //垂直居中
            color:  "transparent"
            Image {
                id:scan_content
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/Scan_content.png" //
                opacity: 1
            }
            //向导说明
            Rectangle {//680*560  (60,130)--(30,30)aspectRatio>1.7?parent.width*0.4:
                id: guideRectangle
                width: parent.width*0.534
                height: parent.height*0.9
                x: parent.width*0.024
                y: parent.height*3/62
                color:"transparent"//"red"
                Image {
                    id:scan_guide
                    anchors.fill: parent // 图像填充整个窗口
                    source: "qrc:/img/Scan_guide.png"//scan_guide
                    opacity: 1//0.2
                }
                // (盒子热点模式)
                Rectangle {//680*280  (60,130)
                    id:wifimodeTitle            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    anchors.top: parent.top         //anchors.topMargin: 10
                    width: guideRectangle.width
                    height: guideRectangle.height / 2-1
                    color: "transparent"
                    Rectangle {//50*50  (90,145)----(30,15)
                        id:wifiModeIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                        width: parent.width*4/68
                        height: wifiModeIcon.width
                        x: parent.width*3/68//aspectRatio>1.7?:parent.width*3/68
                        y: parent.width*1/68//aspectRatio>1.7?parent.height*3/56:parent.height*3/56//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                        color: "transparent"
                        Image {
                            anchors.fill: wifiModeIcon // 背景图像填充整个窗口
                            source: "qrc:/img/guide_wifi.png"//guide_bg
                            fillMode: Image.PreserveAspectFit
                        }
                    }
                    Text {  //155,71-----95
                        text:currentLanguage === "zh" ? qsTr("盒子热点模式"): qsTr("StarMaster Pro Wi-Fi Mode")
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                        color: "white"
                        font.pixelSize: aspectRatio>1.7 ?16:24
                        x: aspectRatio>1.7?parent.width*5/34:parent.width*19/136
                        anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                    }
                    Rectangle {//678*1  (61,210)----(1,80)
                        id:wifiModeLine            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                        width: parent.width-1
                        height: 1
                        x: wifimodeTitle.x+0.5
                        y: wifiModeIcon.height+2*wifiModeIcon.y//aspectRatio>1.7?parent.height*2/7:parent.height*2/7
                        color: "transparent"
                        Image {
                            anchors.fill: parent
                            source: "qrc:/img/Scan_guide_line.png"
                            fillMode: Image.PreserveAspectFit
                        }
                    }
                    //wifi说明内容
                    Rectangle {
                        width: wifimodeTitle.width*0.95
                        height:parent.height-wifiModeIcon.height-3*wifiModeIcon.y
                        y:wifiModeLine.y+wifiModeLine.height+wifiModeIcon.y//aspectRatio>1.7?parent.height*2/7+wifiModeLine.height+1:parent.height*2/7+wifiModeLine.height+1
                        anchors.horizontalCenter: parent.horizontalCenter//水平居中
                        color:"transparent"
                        ScrollView {
                            clip: true  // 如果文本超出区域，则不显示
                            width: parent.width
                            height: parent.height
                                Text {
                                    id: guideWifiText
                                    width: parent.width
                                    height: parent.height
                                    // textFormat: Text.RichText
                                    wrapMode: Text.Wrap  // 允许文本换行
                                    text:currentLanguage === "zh" ? qsTr("* 手机的WiFi设置为设备所发出的热点WiFi即可使用QUARCS应用\n* 注:该WiFi连接无需密码\n* 未找到实体设备时可点击虚拟设备体验,首次打开时间可能较长,请耐心等待"):
                                                                         qsTr("* Connect your phone to the StarMaster Pro's Wi-Fi hotspot to start using the QUARCS app.\n* No password required for connecting to the Wi-Fi hotspot\n* If physical devices are not detected, you can click on virtual devices to explore the application.\n  Please note that opening the application for the first time may take a little longer. Thank you for your patience.")
                                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                    font.pixelSize: aspectRatio>1.7 ?12:18//确保不设置时各平台字体大小一致//font.pixelSize: aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20//anchors.centerIn: parent
                                    anchors.centerIn: parent
                                    color: "white"
                                }
                            // }
                        }
                    }
                    Rectangle {
                        id:wifi_wlan_Line            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                        width: parent.width-1
                        height: 1
                        x: wifimodeTitle.x+0.5
                        y: wifimodeTitle.height
                        color: "transparent"
                        Image {
                            anchors.fill: parent
                            source: "qrc:/img/Scan_guide_line.png"
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                }
                // (局域网模式)
                Rectangle {//680*280  (60,410)
                    id:wlanmodeTitle            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    anchors.bottom: guideRectangle.bottom
                    width: guideRectangle.width
                    height: guideRectangle.height / 2-1
                    color: "transparent"
                    Rectangle {//50*50  (90,425)----(30,15)
                        id:wlanModeIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                        width: parent.width*4/68
                        height: wlanModeIcon.width
                        x: parent.width*3/68
                        y: parent.width*1/68//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                        color: "transparent"
                        Image {
                            anchors.fill: wlanModeIcon // guide_bg背景图像填充整个窗口
                            source: "qrc:/img/guide_wlan.png"
                            fillMode: Image.PreserveAspectFit
                        }
                    }
                    Text {  //155,71-----95
                        text:currentLanguage === "zh" ? qsTr("局域网模式"): qsTr("WLAN Mode")
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                        color: "white"
                        font.pixelSize:aspectRatio>1.7 ?16:24// aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20                   //anchors.centerIn: parent
                        x: aspectRatio>1.7?parent.width*5/34:parent.width*19/136
                        anchors.verticalCenter: wlanModeIcon.verticalCenter //垂直居中
                    }
                    Rectangle {//678*1  (61,210)----(1,80)
                        id:wlanModeLine            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                        width: parent.width-1
                        height: 1
                        x: wlanmodeTitle.x+0.5
                        y: wlanModeIcon.height+2*wlanModeIcon.y
                        color: "transparent"
                        Image {
                            anchors.fill: parent
                            source: "qrc:/img/Scan_guide_line.png"
                            fillMode: Image.PreserveAspectFit
                        }
                    }
                    //wlan说明内容
                    Rectangle {
                        width: parent.width*0.95
                        height:parent.height-1-wlanModeIcon.height-3*wlanModeIcon.y
                        y:wlanModeLine.y+wlanModeLine.height+wlanModeIcon.y
                        anchors.horizontalCenter: parent.horizontalCenter//水平居中
                        color:"transparent"
                        ScrollView {
                            clip: true  // 如果文本超出区域，则不显示
                            width: parent.width
                            height: parent.height
                            // Column {
                                anchors.fill:parent//width: parent.width
                                spacing: 10
                                Text {
                                    id: guideWlanText
                                    wrapMode: Text.Wrap  // 允许文本换行
                                    text:currentLanguage === "zh" ? qsTr("* 设备连接到局域网内,手机连接同局域网的WLAN  即可使用QUARCS应用\n* 未找到实体设备时可点击虚拟设备体验,首次打开时间可能较长,请耐心等待"):
                                                                         qsTr("* Connect your phone and StarMaster Pro to the same WLAN, and QUARCS will work. \n* If physical devices are not detected, you can click on virtual devices to explore the application.\n  Please note that opening the application for the first time may take a little longer. Thank you for your patience.")
                                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                    font.pixelSize: aspectRatio>1.7 ?12:18//确保不设置时各平台字体大小一致//font.pixelSize: aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20//anchors.centerIn: parent
                                    width: parent.width
                                    height: parent.height
                                    anchors.centerIn: parent
                                    color: "white"
                                }
                            // }
                        }
                    }
                }
          }
            //扫描结果 改为按钮，避免点击文字以外的位置无反应
            Button {//324*80   (770,130)--(,30)
                id: result
                width:parent.width*0.254
                height:btn_scan_back.height//parent.height*0.13
                x:guideRectangle.x+guideRectangle.width+guideRectangle.x
                y:guideRectangle.y
                // color:  "transparent"
                Image {
                    id:scan_result
                    anchors.fill: parent
                    source: "transparent"
                }
                background: Image {
                    id:btn_Img_scan_result
                    source: "qrc:/img/Scan_result.png";
                    anchors.fill: parent
                }
                Text {
                    text:currentLanguage === "zh" ? qsTr("未扫到设备 "): qsTr("No Devices Detected")
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    color: "white"
                    font.pixelSize: aspectRatio>1.7 ?14:22//aspectRatio>1.7 ? Screen.width * 0.023 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20                   //anchors.centerIn: parent
                    anchors.centerIn: parent
                }
                onClicked: {
                    ipRectangle.visible=true;
                    scanDialog.enabled=false;
                }
            }
            //扫描按钮
            Button{//170*80
                id: btn_scan
                width:parent.width* 170/1274// *0.14//
                height:result.height
                y:guideRectangle.y
                anchors {
                    right: parent.right
                    rightMargin:guideRectangle.x // 距离右边的比例边距
                }
                Image {
                    id:scan_btn
                    anchors.fill: parent
                    source: "transparent"
                }
                background: Image {
                    id:btn_Img_scan
                    source: "qrc:/img/Btn_bg_scan.png";
                    anchors.fill: parent
                }
                contentItem: Item{
                    width: parent.width
                    height: parent.height
                    anchors.centerIn: parent
                    Row {
                        width:scan_icon.width + btn_scanText.width + 5
                        height: parent.height
                        anchors.centerIn: parent
                        spacing: 5
                        Image {
                            id:scan_icon
                            source: "qrc:/img/Btn_scan.png";
                            fillMode: Image.PreserveAspectFit
                            width: btn_scan.height *5/16
                            height: btn_scan.height *5/16
                            anchors.verticalCenter: parent.verticalCenter //垂直居中      //visible: true // 初始时可见
                        }
                        // 按钮文字部分
                        Text {
                            id:btn_scanText
                            text:currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20
                            color: "white"//verticalAlignment: Text.AlignVCenter//垂直//horizontalAlignment: Text.AlignHCenter//width: parent.width * 0.7  // 只占左侧的 70% 宽度，避免遮挡图案
                            anchors.verticalCenter: parent.verticalCenter //垂直居中//anchors.centerIn: parent
                        }
                    }
                }
                onClicked: {
                    ipList = [];
                    ipVersions = [];
                    updateIPlistshow_color();
                    btn_Img_scan.opacity=0.5//透明
                    // 重置扫描完成标志
                    wifiScanCompleted = false
                    bluetoothScanCompleted = false
                    elapsedTime = 0;  // 重置计时qsTr(elapsedTime)
                    btn_scanText.text=currentLanguage === "zh" ? qsTr("0"): qsTr("0")
                    btn_scan.enabled=false;
                    btn_scan_back.enabled=false;
                    listContent.enabled=false
                    btn_Img_scan_back.opacity=0.1
                    vdVirDevice.enabled=false
                    btn_Img_scan_vd.opacity=0.4

                    timer.start();  // 开始计时器
                    serverFinder.findServerAddress(3000);  // 设置超时时间为 3000 毫秒
                }
            }
            //扫描结果文字提示
            Rectangle {
                id:scanEndText
                width:parent.width*0.28
                height: vdVirDevice.height
                anchors {
                    top:result.bottom
                    topMargin: parent.height*3/62
                    right: content.right
                    rightMargin:(content.width-guideRectangle.x-guideRectangle.width-scanEndText.width)/2 // 距离右边的比例边距
                }
                color:"transparent"
                Text {
                    id:scan_endText
                    text:""
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20
                    color: "white"                    //anchors.verticalCenter: parent.verticalCenter //垂直居中//
                    anchors.centerIn: parent
                }
            }
            //扫描列表
            Rectangle {//330*280
                id:pdPhyDevice
                width:parent.width*0.42//0.261aspectRatio>1.7?parent.width*0.4:
                height: wifimodeTitle.height
                anchors {
                    bottom: vdVirDevice.top
                    bottomMargin: (vdVirDevice.y-scanEndText.y-scanEndText.height-pdPhyDevice.height)/2
                    right: content.right
                    rightMargin:(content.width-guideRectangle.x-guideRectangle.width-pdPhyDevice.width)/2 // 距离右边的比例边距
                }//     anchors.horizontalCenter: parent.horizontalCenter//水平居中
                color: "transparent"//"red"//
                // ScrollView包装Text
                ScrollView {
                    id: deviceScroll
                    clip: true  // 如果文本超出区域，则不显示
                    width: parent.width//*0.58
                    height: parent.height//*0.9
                    contentHeight: listContent.implicitHeight
                    z:1
                    /**/
                    WebSocket {
                        id: webSocket
                        active: false // 默认不自动连接
                        onStatusChanged:function(status) {
                            console.log("WebSocket connected to:", url," status:",status);
                            if (status === WebSocket.Open) {
                                console.log("WebSocket success:", url);
                                // 启动定时器
                                sendTimer.start();
                                //webViewURL.visible = true;
                            }
                            /*else if (status === WebSocket.Error) {
                                console.error("WebSocket error:", errorString)
                                var errorMsg = currentLanguage === "zh" ?
                                            qsTr("QUARCS 连接已断开，可能是 WiFi 网络自动切换导致，请检查网络设置。") :
                                            qsTr("QUARCS connection lost due to WiFi auto-switching. Please verify network configuration.");
                                webFailRectangle.errorDetail = errorMsg; // 更新错误详情
                                elapsedTime = 0;  // 重置计时
                                timer.stop();
                                sendTimer.stop(); // 停止定时器
                                webFailRectangle.visible = true;
                            }*/
                            else if (status === WebSocket.Closed) {
                                sendTimer.stop(); // 连接关闭时停止定时器
                            }
                        }
                    }
/**/
                    Timer {
                        id: sendTimer
                        interval: 10000 // 10秒
                        repeat: false// 使计时器不断重复
                        triggeredOnStart: false // 立即触发一次
                        running: false
                        onTriggered: {
                            if (webSocket.status === WebSocket.Open) {
                                // 发送盒子WiFi重启指令
                                const data = JSON.stringify({
                                                                type: "APP_msg",
                                                                // time: currentTime
                                });

                                if (webSocket.sendTextMessage(data)) {
                                    console.log("webSocket Message sent successfully", data);
                                } else {
                                    console.error("webSocket Failed to send message (connection may not be ready)");
                                }
                            }
                        }
                    }
                    Column {
                        id: ipListContainer
                        width: parent.width
                        spacing: parent.height * 0.02
                        visible: ipList.length > 0
                        enabled: listContent.enabled
                        Repeater {
                            model: ipList.length
                            delegate: Row {
                                width: ipListContainer.width
                                spacing: isBluetoothDevice ? 0 : width * 0.01  // 蓝牙设备时无间距
                                property real textHeight: ipLabel.implicitHeight
                                property string deviceIp: ipList[index]
                                property string versionValue: (ipVersions && ipVersions.length > index && ipVersions[index]) ? ipVersions[index] : ""
                                property string displayText: deviceIp//versionValue ? deviceIp + " (Vh:" + versionValue + ")" : deviceIp
                                // 检查是否是蓝牙设备（通过关键字ESP32-Stepper判断）
                                property bool isBluetoothDevice: deviceIp.indexOf("ESP32-Stepper") >= 0
                                // 判断是否正在扫描（扫描过程中不可用）
                                property bool isScanning: !wifiScanCompleted || !bluetoothScanCompleted
                                height: Math.max(textHeight, btn_scan.height)
                                Rectangle {
                                    width: isBluetoothDevice ? parent.width : parent.width * 0.50
                                    height: ipLabel.implicitHeight
                                    color: "transparent"
                                    opacity: isScanning ? 0.4 : 1.0  // 扫描过程中降低透明度
                                    Text {
                                        id: ipLabel
                                        text: displayText
                                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                        font.pixelSize: aspectRatio>1.7 ?20*aspectRatio/2.2:28
                                        color: deviceIp === selectedIp ? "white" : "#006EFF"
                                        wrapMode: isBluetoothDevice ? Text.NoWrap : Text.Wrap  // 蓝牙设备不换行，WiFi设备允许换行
                                        elide: isBluetoothDevice ? Text.ElideRight : Text.ElideNone  // 蓝牙设备过长时显示省略号
                                        width: parent.width
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: !isScanning  // 扫描过程中禁用点击
                                        onClicked: {
                                            selectedIp = deviceIp;
                                            webViewURL.hasErrorOccurred=false
                                            updateIPlistshow_color();
                                            
                                            // 检查是否是蓝牙设备（通过关键字ESP32-Stepper判断）
                                            var isBluetoothDevice = deviceIp.indexOf("ESP32-Stepper") >= 0;
                                            
                                            if (isBluetoothDevice) {
                                                // 蓝牙设备：连接设备
                                                console.log("Bluetooth device selected: " + deviceIp);
                                                
                                                // 从 deviceIp 中提取 MAC 地址
                                                // 格式："ESP32-Stepper (28:39:26:9C:E3:FE)"
                                                var macAddress = "";
                                                var startIndex = deviceIp.indexOf("(");
                                                var endIndex = deviceIp.indexOf(")");
                                                if (startIndex >= 0 && endIndex > startIndex) {
                                                    macAddress = deviceIp.substring(startIndex + 1, endIndex);
                                                }
                                                
                                                if (macAddress !== "") {
                                                    console.log("Connecting to Bluetooth device: " + macAddress);
                                                    bluetoothScanner.connectToDevice(macAddress);
                                                } else {
                                                    console.log("Failed to extract MAC address from: " + deviceIp);
                                                    showTemporaryTip(currentLanguage === "zh" ? 
                                                        qsTr("无法提取设备地址，请重新扫描") : 
                                                        qsTr("Failed to extract device address. Please rescan."));
                                                }
                                            } else {
                                                // IP地址设备：正常打开HTTP URL
                                                webDialog.visible=true
                                                webViewURL.url = "http://" + selectedIp + ":8080";
                                            }
                                        }
                                    }
                                }
                                Button {
                                    id: deviceActionButton
                                    property bool hasLocalPackage: deviceHasLocalPackages && deviceHasLocalPackages.length > index ? deviceHasLocalPackages[index] : false
                                    property string localPkgPath: deviceLocalPaths && deviceLocalPaths.length > index ? deviceLocalPaths[index] : ""
                                    property bool hasUploaded: deviceUploadStatusMap && deviceUploadStatusMap[deviceIp] === true
                                    width: parent.width * 0.49
                                    height: btn_scan.height
                                    visible: !isBluetoothDevice  // 蓝牙设备不显示更新按钮
                                    enabled: !isScanning && !deviceActionButton.hasUploaded  // 扫描过程中禁用，已上传也禁用
                                    opacity: isScanning ? 0.4 : 1.0  // 扫描过程中降低透明度
                                    background: Image {
                                        source: "qrc:/img/Btn_bg_scan.png";
                                        anchors.fill: parent
                                    }
                                    contentItem: Text {
                                        text: deviceActionButton.hasUploaded ?
                                              (currentLanguage === "zh" ? qsTr("已上传") : qsTr("Uploaded")) :
                                              (deviceActionButton.hasLocalPackage ?
                                                   (currentLanguage === "zh" ? qsTr("上传设备") : qsTr("Upload to Device")) :
                                                   (currentLanguage === "zh" ? qsTr("检查更新") : qsTr("Check for Updates")))
                                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                        font.pixelSize: aspectRatio>1.7 ?16*aspectRatio/2.11:24
                                        color: "white"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        anchors.fill: parent
                                    }
                                    onClicked: {
                                        selectedIp = deviceIp;
                                        updateIPlistshow_color();
                                        var vh = versionValue ? versionValue.trim() : ""
                                        if (vh === "") {
                                            showTemporaryTip(currentLanguage === "zh" ?
                                                 qsTr("未获得有效的 QUARCS 版本号，请重新扫描设备。") :
                                                 qsTr("No valid QUARCS version detected. Please rescan the device."))
                                            return
                                        }
                                        if (vh==="Unknown") {
                                            showTemporaryTip(currentLanguage === "zh" ?
                                                 qsTr("QUARCS当前版本不支持远程更新，需手动烧录。") :
                                                 qsTr("Current QUARCS build does not support remote updates. Please flash manually."))
                                            return
                                        }
                                        if (!majorVersions || majorVersions.length === 0) {
                                            showTemporaryTip(currentLanguage === "zh" ?
                                                 qsTr("请切换到可联网的 Wi-Fi 获取最新版本信息。") :
                                                 qsTr("Switch to an online Wi-Fi to fetch latest versions."))
                                            //websocket 发送指令使得盒子重启WiFi
                                            //webSocket.url = "ws://" + selectedIp + ":8600/";
                                            // webSocket.active = true;
                                            return
                                        }
                                        var versionInfo = getLatestVersionForMajor(vh, majorVersions)
                                        console.log("onClicked : Vh:",vh,"Vx:",versionInfo.version," hasHigherMajor :",versionInfo.hasHigherMajor)     //var vx = versionInfo.version
                                        if (versionInfo.version === "") {
                                            showTemporaryTip(currentLanguage === "zh" ?
                                                 qsTr("未找到对应的大版本最新版本号。") :
                                                 qsTr("No latest version found for this major release."))
                                            return
                                        }
                                        var diff = compareVersions(vh, versionInfo.version)
                                        if (diff >= 0) {
                                            if (versionInfo.hasHigherMajor) {
                                                showTemporaryTip(currentLanguage === "zh" ?
                                                qsTr("QUARCS有大版本更新，需手动烧录。") :
                                                qsTr("A newer major QUARCS release is available. Please flash manually."))
                                                return
                                            }else{
                                                showTemporaryTip(currentLanguage === "zh" ?
                                                qsTr("QUARCS已是最新版本。") :
                                                qsTr("Device is already up to date."))
                                                return
                                            }
                                        }

                                        if (deviceActionButton.hasUploaded) {
                                            showTemporaryTip(currentLanguage === "zh" ?
                                                             qsTr("该设备更新包已上传，可在设备页面执行一键更新。") :
                                                             qsTr("Package already uploaded. Open the device page to run the one-click update."))
                                            return
                                        }

                                        if (deviceActionButton.hasLocalPackage) {
                                            var localPath = deviceActionButton.localPkgPath || ""
                                            if (localPath === "") {
                                                showTemporaryTip(currentLanguage === "zh" ?
                                                                 qsTr("未找到本地更新包，请重新下载。") :
                                                                 qsTr("Local update package missing. Please redownload."))
                                                return
                                            }
                                            updateDialog.mode = "upload"
                                            updateDialog.uploadFilePath = localPath
                                            updateDialog.uploadTargetIp = selectedIp
                                            updateDialog.resetUI()
                                            scanDialog.visible = false
                                            updateDialog.visible = true
                                            return
                                        }

                                        console.log("onClicked :", deviceIp," Vh:",versionValue," Vgroup:",majorVersions)

                                        versionWEBValue = versionInfo.version
                                        console.log("Need update: Vh:", vh, "Vx:", versionInfo.version)
                                        updateDialog.mode = "download"
                                        updateDialog.resetUI()
                                        scanDialog.visible = false
                                        updateDialog.visible = true

                                    }
                                }
                            }
                        }
                    }
                    // 显示设备列表，条件是 ipList 不为空或显示提示
                    Text {
                        id: listContent
                        // z: 1
                        textFormat: Text.RichText
                        wrapMode: Text.Wrap  // 允许文本换行
                        text:""
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                        font.pixelSize: aspectRatio>1.7 ?20:28//aspectRatio>1.7? Screen.width * 0.04 : Screen.width * 0.04
                        width: parent.width
                        height:implicitHeight// parent.height
                        anchors.centerIn: parent  // 确保文本居中显示
                        visible: false//ipList.length > 0  // 当有设备时显示设备列表
                        onLinkActivated:(link) =>{
                            // if (webSocket.active) {
                            //     webSocket.active = false; // 关闭旧连接
                            // }
                            selectedIp = link;// 更新选中的 IP 地址
                            webViewURL.hasErrorOccurred=false
                            //更新显示打开选中IP网页
                            updateIPlistshow_color();
                            webDialog.visible=true
                            webViewURL.url = "http://" + selectedIp + ":8080";
                        }
                    }
                }

                // 显示图标，条件是 ipList 为空
                Image {
                    id: noDeviceImage
                    source: "qrc:/img/Nolist.png"  // 图标路径
                    width: parent.width
                    height: parent.height
                    anchors.centerIn: parent
                    visible: ipList.length === 0  // 当没有设备时显示图标
                }
            }
            //虚拟设备 改为按钮，避免点击文字以外的位置无反应
            Button{//330*60
                id: vdVirDevice
                width:parent.width*0.26
                height:btn_scan_back.height//parent.height*0.11
                visible: ipList.length === 0  // 当设备列表为空时显示虚拟设备
                anchors {
                    bottom: guideRectangle.bottom
                    right: content.right
                    rightMargin:(content.width-guideRectangle.x-guideRectangle.width-vdVirDevice.width)/2 // 距离右边的比例边距
                }
                Image {
                    id:scan_vd
                    anchors.fill: parent
                    source: "transparent"
                }
                background: Image {
                    id:btn_Img_scan_vd
                    source: "qrc:/img/VirtualDevice.png";
                    anchors.fill: parent
                }
                Text {
                    id:vdContent
                    textFormat: Text.RichText
                    text:currentLanguage === "zh" ? qsTr("<a href='virtual_device_link' style='text-decoration: none;'><font color='#006EFF'>虚拟设备 ></font></a>"):
                                                    qsTr("<a href='virtual_device_link'  style='text-decoration: none;'><font color='#006EFF'>Virtual Device ></font></a>")
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    color: "white"
                    font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.023 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20                   //anchors.centerIn: parent
                    anchors.centerIn: parent
                }
                onClicked: {
                    selectedIp = "8.211.156.247";
                    webDialog.visible=true
                    webViewURL.hasErrorOccurred=false
                    webViewURL.url ="http://" + selectedIp + ":8080";
                }
            }
        }
        onVisibleChanged:{
            if(visible){
                selectedIp=""
                updateIPlistshow_color();
                btn_scanText.text=currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan")
                // btn_scan.clicked()//避免更换wifi后列表不更新问题
                scanDialog.enabled=true;
                startDialog.visible=false;
                //区分检查更新按钮显示内容函数
                if (hasFetchedVersions) {
                    checkLocalUpdatePackages()
                }
            }
        }
    }

    //IP提示框
    Rectangle {
        id: ipRectangle
        visible: false
        anchors.fill: parent
        color:  "transparent"
        Image {
            id:bg_IP
            anchors.fill: parent
            source: "qrc:/img/IP_bg.png"//scan_IPbg   Scan_result
            fillMode: Image.Stretch // 拉伸模式//  fillMode: Image.PreserveAspectFit//
            opacity:1
        }
        Rectangle {
            id: ipConncet//visible: false
            color:  "transparent"
            width: parent.width*520/1334
            height: aspectRatio>1.7 ? parent.height*411/750:parent.height*261/750//parent.height*411/750
            anchors.centerIn: parent//anchors.horizontalCenter: parent.horizontalCenter//anchors.verticalCenter: parent.verticalCenter
            Image {            //id:bg_IP
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/IP_ImgBg.png"
                fillMode: Image.Stretch//fillMode: Image.PreserveAspectFit
                opacity:1
            }
            //IP图标
            Rectangle {
                id:ipIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                width: parent.width*5/52
                height: ipIcon.width
                x: parent.width*3/52
                y: aspectRatio>1.7 ? parent.height*15/411:parent.height*7/261 //parent.height*15/411
                color: "transparent"
                Image {
                    anchors.fill: ipIcon // guide_bg背景图像填充整个窗口
                    source: "qrc:/img/IP_icon.png"
                    fillMode: Image.PreserveAspectFit
                }
            }
            //IP文字标题
            Text {
                text:currentLanguage === "zh" ? qsTr("请输入设备IP"): qsTr("Input Device IP")
                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                color: "white"
                font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 //anchors.centerIn: parent
                x:parent.width*108/520
                anchors.verticalCenter: ipIcon.verticalCenter //垂直居中
            }
            //IP分割线
            Rectangle {
                id:ipLine            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                width: parent.width-2//ipConncet.width*518/520//ipRectangle.width*518/1334//parent.width-2
                height: 1
                anchors{
                    left:parent.left
                    right: parent.right
                    leftMargin: 1
                    rightMargin: 1
                }
                y: parent.height*80/411
                color: "transparent"
                Image {
                    anchors.fill: parent
                    source: "qrc:/img/IP_line.png"
                    fillMode: Image.Stretch//
                    //fillMode: Image.PreserveAspectFit
                }
            }
            //IP输入框
            Rectangle {
                width: parent.width*400/520
                height:parent.height*80/411
                x:parent.width*60/520
                y:parent.height*141/411
                anchors.horizontalCenter: parent.horizontalCenter//水平居中
                color:"transparent"
                Image {            //id:bg_IP
                    anchors.fill: parent // 图像填充整个窗口
                    source: "qrc:/img/IP_input.png"
                    fillMode: Image.Stretch//fillMode: Image.PreserveAspectFit
                    opacity:1
                }
                TextInput {
                    id: ipAddressInput
                    anchors.fill: parent
                    font.pixelSize: aspectRatio>1.7 ?16:20//16//Screen.width * 0.024//34  //focus: visible
                    text: "" // 初始化为空
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    inputMethodHints: Qt.ImhPreferNumbers // 数字键盘
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    padding: 0
                    // 当内容改变时更新按钮状态
                    onTextChanged: {
                        // 使用正则表达式移除汉字字符var newText = ipAddressInput.text.replace(/[\u4e00-\u9fa5。]/g, ''); // 移除所有汉字字符
                        var newText = ipAddressInput.text.replace(/[^0-9.]/g, ''); // 只允许数字和点号
                        ipAddressInput.text = newText; // 更新文本内容
                        // 当输入框内容发生变化时，判断是否输入超过最大长度
                        if (ipAddressInput.text.length > 15) {
                            ipAddressInput.text = ipAddressInput.text.substring(0, 15); // 只保留前15个字符
                        }
                        ipButton.enabled = validateIP(ipAddressInput.text.trim())
                    }
                }
                Text {
                    anchors.centerIn: parent//anchors.fill: ipAddressInput
                    font.pixelSize:aspectRatio>1.7 ?16:20//Screen.width * 0.024
                    text: currentLanguage === "zh" ? qsTr("示例:") + "192.168.1.1" : qsTr("e.g. ") + "192.168.1.1"
                    color: "grey"
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    visible: ipAddressInput.text.trim() === "" // 当输入框为空时显示
                }
            }
            //取消按钮
            Button {
                id: cancelButton
                width:parent.width*180/520
                height:parent.height*80/411
                y:parent.height*271/411
                anchors {
                    left: parent.left
                    leftMargin:parent.width*60/520
                }
                Image {
                    id:btn_cancel
                    anchors.fill: parent
                    source: "transparent"
                }
                background: Image {
                    id:btn_Img_IP
                    source: "qrc:/img/IP_Btn_enable.png";
                    fillMode: Image.Stretch//fillMode: Image.PreserveAspectFit
                    anchors.fill: parent
                }
                Text{
                    text: currentLanguage === "zh" ? qsTr("取消"): qsTr("Cancel")
                    color: "white"
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    font.pixelSize:aspectRatio>1.7 ?16:24//Screen.width * 0.025
                    anchors.centerIn: parent
                }
                onClicked: {
                    ipRectangle.visible = false
                    scanDialog.enabled=true
                }
            }
            //连接按钮
            Button {
                id: ipButton
                enabled: false
                width:parent.width*180/520
                height:parent.height*80/411
                y:parent.height*271/411
                anchors {
                    right: parent.right
                    rightMargin:parent.width*60/520
                }
                Image {
                    id:btn_conncet
                    anchors.fill: parent
                    source: "transparent"
                }
                background: Image {
                    id:btn_Img_IPc
                    source: ipButton.enabled===true?"qrc:/img/IP_Btn_enable.png":"qrc:/img/IP_Btn_disable.png"
                    fillMode: Image.Stretch//fillMode: Image.PreserveAspectFit
                    anchors.fill: parent
                }
                Text{
                    text: currentLanguage === "zh" ? qsTr("连接") : qsTr("Conncet")
                    color: "white"
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    font.pixelSize: aspectRatio>1.7 ?16:24//Screen.width * 0.025
                    anchors.centerIn: parent
                }
                onClicked: {
                    var userInput = ipAddressInput.text.trim();
                    if (validateIP(userInput)) {
                        scanDialog.visible=false;
                        ipRectangle.visible=false;
                        // if (webSocket.active) {
                        //     webSocket.active = false; // 关闭旧连接
                        // }
                        selectedIp=userInput
                        webViewURL.hasErrorOccurred=false
                        webDialog.visible=true
                        webViewURL.url = "http://" + selectedIp +":8080";
                        elapsedTime = 0;  // 重置计时
                        timer.start();  // 开始计时器
                    }
                }
            }
        }
    }

    //更新包下载页面
    Rectangle {
        id: updateDialog
        property string mode: "download"  // 或 "upload"
        property string uploadFilePath: ""
        property string uploadTargetIp: ""
        visible: false
        anchors.fill: parent
        color: "transparent"

        function resetUI() {
            setUploadButtonsEnabled(true)
            if (btn_update_back) {
                btn_update_back.enabled = true
                btn_Img_update_back.opacity=1
            }
            if (updateDownloadProgress) {
                updateDownloadProgress.visible = false
                updateDownloadProgress.value = 0
            }
            if (updateProgressText) {
                updateProgressText.visible = false
                updateProgressText.text = ""
            }
            if (updateDownloadStatusText) {
                updateDownloadStatusText.visible = false
                updateDownloadStatusText.text = ""
            }
            if (updateDialogPathText) {
                updateDialogPathText.visible = false
                updateDialogPathText.text = ""
            }
        }

        function setUploadButtonsEnabled(enabled) {
            if (btn_updateDownload) {
                btn_updateDownload.enabled = enabled
            }
            if (btn_wsUpload) {
                btn_wsUpload.enabled = enabled
            }
        }

        function validateUploadInputs() {
            if (!updateDialog.uploadFilePath || updateDialog.uploadFilePath === "") {
                showTemporaryTip(currentLanguage === "zh" ?
                                 qsTr("未找到本地更新包，请重新下载。") :
                                 qsTr("Local update package missing. Please re-download."))
                return false
            }
            if (!updateDialog.uploadTargetIp || updateDialog.uploadTargetIp === "") {
                showTemporaryTip(currentLanguage === "zh" ?
                                 qsTr("未选择目标设备。") :
                                 qsTr("No target device selected."))
                return false
            }
            return true
        }

        function prepareUploadUI(initialStatus) {
            updateDownloadProgress.visible = true
            updateDownloadProgress.value = 0
            contentItem_downloadProgress.visible = true
            updateProgressText.visible = true
            updateProgressText.text = ""
            updateDownloadStatusText.visible = true
            updateDownloadStatusText.text = initialStatus
            updateDialogPathText.visible = false
            setUploadButtonsEnabled(false)
            if (btn_update_back) {
                btn_update_back.enabled = false
                btn_Img_update_back.opacity = 0.1
            }
        }

        //标题栏
        Rectangle {//1334*70  (0,0)
            id: updatetitle
            width:parent.width
            height:aspectRatio>1.7?parent.height*7/75:parent.height*5/75
            x:0
            y:0
            color:  "transparent"
            Image {
                id:update_title
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/Scan_title.png" //
                opacity:1
            }
            //返回按钮
            Button{//17*30   (30,20)
                id:btn_update_back
                width: parent.width*47/1334
                height: parent.height
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                }
                background: Rectangle {
                    color: "transparent" // 背景色设置为透明
                }
                Image {
                    id: btn_Img_update_back
                    source: "qrc:/img/Btn_back.png"
                    width: title.width*0.013//30 // 图标的大小
                    height: title.height*3/7//30
                    anchors {
                        right: parent.right // 图标靠右
                        verticalCenter: parent.verticalCenter // 图标垂直居中
                        margins: 5
                    }
                }
                onClicked: {
                    updateDialogAutoCloseTimer.stop()
                    updateDialogAutoCloseTimer.start()
                    updateDialog.visible=false;
                    scanDialog.visible=true;
                }
            }
            //标题文字
            Text {
                anchors.centerIn: parent
                text:updateDialog.mode === "upload"?(currentLanguage === "zh" ? qsTr("上传"): qsTr("Upload")):(currentLanguage === "zh" ? qsTr("更新"): qsTr("Update"))
                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                color: "white"
                font.pixelSize: aspectRatio>1.7 ?16:24
            }
        }

        //更新包下载按钮与状态显示
        Rectangle {
            id: updateContent
            width:parent.width*0.955
            height:aspectRatio>1.7?parent.height*62/75:parent.height*66/75//parent.height*0.826
            y:aspectRatio>1.7?parent.height*10/75:parent.height*7/75//parent.height*2/15
            anchors.horizontalCenter: parent.horizontalCenter//水平居中
            color: "transparent"
            Image {
                anchors.fill: parent
                source: "qrc:/img/Scan_content.png"
                opacity: 1
            }
            Text {
                text: updateDialog.mode === "upload" ?
                      (currentLanguage === "zh" ? qsTr("点击下方按钮将本地更新包发送到选定设备。上传完成后，自动返回扫描页面。需点击IP进入设备手动执行一键更新操作。") :
                                                 qsTr("Tap the button below to upload the local update package to the selected device. After the upload is complete, the app will automatically return to the scan page. Please tap the device IP to enter and manually perform the one-click update.")) :
                      (currentLanguage === "zh" ? qsTr("请切换可以正常连接网络上网的WiFi，然后点击下方按钮获取最新更新包。下载完成后，自动返回扫描页面，若无设备显示请连接QUARCS的WiFi") :
                                                 qsTr("Please switch to a Wi-Fi network with internet access, then tap the button below to download the latest update package. After the download is complete, the app will automatically return to the scan page. If no devices are displayed, please connect to the QUARCS Wi-Fi."))
                wrapMode: Text.Wrap
                color: "white"
                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                font.pixelSize: aspectRatio>1.7 ?14:20
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }
            Column {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 10  // 距离底部 20px
                spacing: 5
                visible: true
                id: downloadColumn
                width: parent.width
                //anchors.horizontalCenter: parent.horizontalCenter

                Row {
                    id: uploadButtonRow
                    spacing: aspectRatio>1.7 ? 30 : 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    Button {
                        id: btn_updateDownload
                        property url downloadUrl: versionWEBValue && versionWEBValue !== "" ?
                                                    ("https://www.qhyccd.cn/file/repository/latestSoftAndDirver/QUARCS/" + versionWEBValue + ".zip") : ""
                        width: aspectRatio>1.7 ? updateContent.width*280/1274*2.11/aspectRatio:updateContent.width*210/1274
                        height: aspectRatio>1.7 ? btn_update_back.height*1.5:btn_update_back.height
                        Image {
                            id:download_btn
                            source: "transparent"
                        }
                        background: Image {
                            id: btn_Img_updateDownload
                            source: "qrc:/img/Btn_bg_scan.png"
                        }
                        Text {
                            id:btn_downloadText
                            text:updateDialog.mode === "upload"?(currentLanguage === "zh" ? qsTr("上传更新包到设备"): qsTr("Upload update zip")):(currentLanguage === "zh" ? qsTr("下载设备更新包"): qsTr("Download update zip"))
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            font.pixelSize: aspectRatio>1.7 ?16:20
                            color: "white"
                            anchors.centerIn: parent
                        }
                        onClicked: {
                            if (updateDialog.mode === "upload") {
                                if (!updateDialog.validateUploadInputs()) {
                                    return
                                }
                                console.log("Upload--- button clicked", updateDialog.uploadTargetIp, updateDialog.uploadFilePath)
                                updateDialog.prepareUploadUI(currentLanguage === "zh" ? qsTr("准备上传...") : qsTr("Preparing upload..."))
                                var uploadUrl = "http://" + updateDialog.uploadTargetIp + ":8000"
                                serverFinder.startUpload(uploadUrl, updateDialog.uploadFilePath)
                            } else {
                                if (!downloadUrl || downloadUrl === "") {
                                    showTemporaryTip(currentLanguage === "zh" ?
                                                     qsTr("未配置下载地址。") :
                                                     qsTr("Download URL is not configured."))
                                    return
                                }
                                console.log("Download--- button clicked");
                                serverFinder.startDownload(downloadUrl)
                                updateProgressText.visible = true
                                updateDownloadStatusText.visible = true
                                updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("准备下载...") : qsTr("Preparing download...")
                                updateDownloadProgress.visible = true
                                updateDownloadProgress.value = 0
                                updateDialogPathText.visible = false
                                btn_updateDownload.enabled = false;
                                btn_update_back.enabled=false;
                                btn_Img_update_back.opacity=0.1
                            }
                        }
                    }
                    Button {
                        id: btn_wsUpload
                        visible: false//updateDialog.mode === "upload"
                        enabled: false//updateDialog.mode === "upload"
                        width: btn_updateDownload.width
                        height: btn_updateDownload.height
                        Image {
                            source: "transparent"
                        }
                        background: Image {
                            source: "qrc:/img/Btn_bg_scan.png"
                        }
                        Text {
                            text: currentLanguage === "zh" ? qsTr("websocket上传") : qsTr("WebSocket upload")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            font.pixelSize: aspectRatio>1.7 ?16:20
                            color: "white"
                            anchors.centerIn: parent
                        }
                        onClicked: {
                            if (!updateDialog.validateUploadInputs()) {
                                return
                            }
                            console.log("WebSocket Upload--- button clicked", updateDialog.uploadTargetIp, updateDialog.uploadFilePath)
                            updateDialog.prepareUploadUI(currentLanguage === "zh" ? qsTr("准备通过 WebSocket 上传...") : qsTr("Preparing WebSocket upload..."))
                            var wsUrl = "ws://" + updateDialog.uploadTargetIp + ":8600/upload"
                            serverFinder.startWebSocketUpload(wsUrl, updateDialog.uploadFilePath)
                        }
                    }
                }
                ProgressBar {
                    id: updateDownloadProgress
                    width: parent.width
                    height: 8
                    from: 0
                    to: 1
                    value:0
                    background: Rectangle {
                        color: "#303030"  //lightgray #303030深灰色背景
                        radius: 3
                    }
                    contentItem: Rectangle {
                        id:contentItem_downloadProgress
                        visible: false
                        width:(updateDownloadProgress.value <= 0) ? 0 : updateDownloadProgress.visualPosition * parent.width
                        height: parent.height
                        color: "#2196F3"  // 蓝色进度
                        radius: 3
                    }
                }
                /*// RowLayout {
                //     width: parent.width
                //     spacing: 10
                //     Text {
                //         id: updateProgressText
                //         visible: false
                //         horizontalAlignment: Text.AlignLeft
                //         text:""
                //         color: "white"
                //         font.pixelSize: aspectRatio>1.7 ?16:24
                //         //Layout.fillWidth: true
                //         elide: Text.ElideRight
                //     }
                //     Text {
                //         id: updateDownloadStatusText
                //         visible:false
                //         Layout.fillWidth: true
                //         horizontalAlignment: Text.AlignHCenter
                //         text:""
                //         color: "white"
                //         font.pixelSize: aspectRatio>1.7 ?16:24
                //         elide: Text.ElideRight
                //     }
                // }*/
                Text {
                    id: updateDialogPathText
                    width: parent.width
                    visible: false
                    wrapMode: Text.Wrap
                    color: "white"
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    font.pixelSize: aspectRatio>1.7 ?14:20
                }
                // 使用 RowLayout 让两个 Text 水平排列，并自动调整宽度
                RowLayout {
                    width: parent.width
                    spacing: 10
                    Text {
                        id: updateProgressText
                        visible: false //true
                        horizontalAlignment: Text.AlignLeft  // 左对齐
                        text:""
                        color: "white"
                        font.pixelSize: aspectRatio>1.7 ?16:24
                        elide: Text.ElideRight   // 如果空间不足，省略右侧内容
                    }
                    Text {
                        id: updateDownloadStatusText
                        visible:false //true
                        Layout.fillWidth: true  // 占据剩余空间
                        horizontalAlignment: Text.AlignHCenter
                        text:""
                        color: "white"
                        font.pixelSize: aspectRatio>1.7 ?16:24
                        elide: Text.ElideRight   // 如果空间不足，省略右侧内容
                    }
                }
            }
        }

        Connections {
            target: serverFinder
            function onDownloadFinished(filePath) {
                if (!updateDialog.visible || updateDialog.mode !== "download")
                    return;
                btn_updateDownload.enabled=true
                btn_update_back.enabled=true
                btn_Img_update_back.opacity=1
                updateProgressText.visible=false
                updateDownloadProgress.visible = false
                updateDownloadStatusText.visible=true
                updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("下载完成") : qsTr("Download completed")
                updateDownloadProgress.value = 1
                updateDialogPathText.visible = true
                updateDialogPathText.text = currentLanguage === "zh" ?
                        qsTr("✓   下载已完成。\n请连接设备网络，将更新包发送给指定设备。\n更新包存储位置: %1").arg(filePath) :
                        qsTr("✓   Download complete.\nConnect to the device network and transfer the package.\nSaved at: %1").arg(filePath)
                updateDialogAutoCloseTimer.stop()
                updateDialogAutoCloseTimer.start()
            }
            function onDownloadErrorOccurred(error) {
                if (!updateDialog.visible || updateDialog.mode !== "download")
                    return;
                btn_updateDownload.enabled=true
                btn_update_back.enabled = true
                btn_Img_update_back.opacity=1
                updateDownloadProgress.value = 0
                updateDownloadProgress.visible = false
                updateProgressText.visible=false
                updateDownloadStatusText.visible = true
                updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("下载错误: %1").arg(error) : qsTr("Download error: %1").arg(error)
                updateDialogPathText.visible = false
            }
            function onProgressChanged(progress, bytesReceived, bytesTotal) {
                if (!updateDialog.visible || updateDialog.mode !== "download")
                    return;
                contentItem_downloadProgress.visible=true
                updateDownloadProgress.value = progress;
                updateProgressText.visible = true;
                updateProgressText.text = Math.round(progress * 100) + "% (" +
                            formatFileSize(bytesReceived) + " / " +
                            formatFileSize(bytesTotal) + ")";
                updateDownloadStatusText.visible = true
                updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("下载中...") : qsTr("Downloading...")
                btn_updateDownload.enabled=false
            }
            function onStatusChanged() {
                if (!updateDialog.visible || updateDialog.mode !== "download")
                    return;
                if (serverFinder.status === "Cancelled") {
                    btn_updateDownload.enabled = true
                    btn_update_back.enabled = true
                    btn_Img_update_back.opacity=1
                    updateDownloadStatusText.visible = true
                    updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("下载已取消") : qsTr("Download cancelled")
                    updateProgressText.visible = false
                    updateDownloadProgress.visible = false
                    updateDialogPathText.visible = false
                }
            }
            function onUploadProgressChanged(progress, bytesSent, bytesTotal) {
                if (!updateDialog.visible || updateDialog.mode !== "upload")
                    return;
                contentItem_downloadProgress.visible = true
                updateDownloadProgress.visible = true
                updateDownloadProgress.value = progress
                updateProgressText.visible = true
                updateProgressText.text = Math.round(progress * 100) + "% (" +
                        formatFileSize(bytesSent) + " / " +
                        formatFileSize(bytesTotal) + ")"
                updateDownloadStatusText.visible = true
                updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("上传中...") : qsTr("Uploading...")
            }
            function onUploadFinished(response) {
                if (!updateDialog.visible || updateDialog.mode !== "upload")
                    return;
                updateDialog.setUploadButtonsEnabled(true)
                btn_update_back.enabled = true
                btn_Img_update_back.opacity=1
                updateDownloadProgress.visible = false
                updateProgressText.visible = false
                updateDownloadStatusText.visible = true
                updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("上传完成") : qsTr("Upload completed")
                updateDialogPathText.visible = true
                updateDialogPathText.text = currentLanguage === "zh" ?
                        qsTr("✓   已成功上传至设备 %1。").arg(updateDialog.uploadTargetIp) :
                        qsTr("✓   Package uploaded to %1.").arg(updateDialog.uploadTargetIp)
                markDeviceUploaded(updateDialog.uploadTargetIp)
                updateDialogAutoCloseTimer.stop()
                updateDialogAutoCloseTimer.start()
            }
            function onUploadErrorOccurred(error) {
                if (!updateDialog.visible || updateDialog.mode !== "upload")
                    return;
                updateDialog.setUploadButtonsEnabled(true)
                btn_update_back.enabled = true
                btn_Img_update_back.opacity=1
                updateDownloadProgress.visible = false
                updateProgressText.visible = false
                updateDownloadStatusText.visible = true
                updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("上传错误: %1").arg(error) : qsTr("Upload error: %1").arg(error)
                updateDialogPathText.visible = false
            }
            function onUploadStatusChanged() {
                if (!updateDialog.visible || updateDialog.mode !== "upload")
                    return;
                if (serverFinder.uploadStatus === "Cancelled") {
                    updateDialog.setUploadButtonsEnabled(true)
                    btn_update_back.enabled = true
                    btn_Img_update_back.opacity=1
                    updateDownloadStatusText.visible = true
                    updateDownloadStatusText.text = currentLanguage === "zh" ? qsTr("上传已取消") : qsTr("Upload cancelled")
                    updateProgressText.visible = false
                    updateDownloadProgress.visible = false
                    updateDialogPathText.visible = false
                }
            }
        }
    }

    //蓝牙控制页面
    Rectangle {
        id: bluetoothControlDialog
        visible: false
        anchors.fill: parent
        color: "transparent"
        
        // 停止命令状态跟踪
        property int stopCommandsSent: 0  // 记录已发送的停止命令数量
        property bool emergencyStopInProgress: false  // 紧急停止标志
        
        // 当对话框显示时，重置各控件状态
        onVisibleChanged: {
            if (visible) {
                // 如果设备已连接，先执行停止操作，暂停设备的所有指令
                if (bluetoothScanner.isConnected) {
                    emergencyStop()
                } else {
                    // 如果设备未连接，重置紧急停止状态
                    emergencyStopInProgress = false
                    stopCommandsSent = 0
                }
                
                // 重置 Target 模式状态
                bluetooth_content.isTargetMode = false
                
                // 重置速度级别
                btn_speed.speedLevel = 1
                
                // 重置 PARK 状态
                btn_park.isParked = false
                
                // 清空命令日志
                commandLogModel.clear()
            }
        }
        
        // 本地辅助函数 - 发射信号到父窗口
        function logCommand(cmd, type) {
            appWindow.commandLogRequested(cmd, type);
        }
        
        // 紧急停止函数 - 确保所有运动都停止
        function emergencyStop() {
            if (!bluetoothScanner.isConnected) {
                return;
            }
            
            emergencyStopInProgress = true;
            stopCommandsSent = 0;
            
            // 发送第一个停止命令
            appWindow.addCommandLog("EMERGENCY STOP - stop_ud", "sent");
            bluetoothScanner.sendCommand("stop_ud");
            stopCommandsSent++;
            
            // 启动定时器发送第二个命令
            emergencyStopTimer.start();
        }
        
        // 紧急停止定时器
        Timer {
            id: emergencyStopTimer
            interval: 200  // 200ms后发送第二个命令
            running: false
            repeat: true
            triggeredOnStart: false
            
            onTriggered: {
                if (!bluetoothScanner.isConnected) {
                    stop();
                    bluetoothControlDialog.emergencyStopInProgress = false;
                    return;
                }
                
                if (bluetoothControlDialog.stopCommandsSent === 1) {
                    // 发送第二个停止命令
                    appWindow.addCommandLog("EMERGENCY STOP - stop_rl", "sent");
                    bluetoothScanner.sendCommand("stop_rl");
                    bluetoothControlDialog.stopCommandsSent++;
                } else if (bluetoothControlDialog.stopCommandsSent === 2) {
                    // 两个命令都已发送，再次发送确保停止（冗余保护）
                    appWindow.addCommandLog("STOP CONFIRM - stop_ud", "sent");
                    bluetoothScanner.sendCommand("stop_ud");
                    bluetoothControlDialog.stopCommandsSent++;
                } else if (bluetoothControlDialog.stopCommandsSent >= 3) {
                    // 完成停止序列
                    stop();
                    bluetoothControlDialog.emergencyStopInProgress = false;
                    bluetoothControlDialog.stopCommandsSent = 0;
                }
            }
        }
        
        //标题栏
        Rectangle {
            id: bluetooth_title
            width: parent.width
            height: aspectRatio > 1.7 ? parent.height * 7/75 : parent.height * 5/75
            x: 0
            y: 0
            color: "transparent"
            Image {
                id: bluetooth_title_bg
                anchors.fill: parent
                source: "qrc:/img/Scan_title.png"
                opacity: 1
            }
            //返回按钮
            Button {
                id: btn_bluetooth_back
                width: parent.width * 47/1334
                height: parent.height
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                }
                background: Rectangle {
                    color: "transparent"
                }
                Image {
                    id: btn_Img_bluetooth_back
                    source: "qrc:/img/Btn_back.png"
                    width: bluetooth_title.width * 0.013
                    height: bluetooth_title.height * 3/7
                    anchors {
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        margins: 5
                    }
                }
                onClicked: {
                    // 断开蓝牙设备连接
                    if (bluetoothScanner.isConnected) {
                        console.log("Disconnecting Bluetooth device before returning to scan page");
                        bluetoothScanner.disconnectDevice();
                    }
                    // 返回扫描页面
                    bluetoothControlDialog.visible = false
                    scanDialog.visible = true
                    elapsedTime = 0;  // 重置计时
                    timer.stop();  // 停止计时器
                    btn_scanText.text = currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan");
                    btn_scan.enabled = true;
                    btn_Img_scan.opacity=1;
                    btn_scan_back.enabled=true;
                    btn_Img_scan_back.opacity=1;
                }
            }
            //标题文字
            Text {
                id: bluetooth_title_text
                anchors.centerIn: parent
                text: currentLanguage === "zh" ? qsTr("蓝牙控制") : qsTr("Bluetooth Control")
                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                color: "white"
                font.pixelSize: aspectRatio > 1.7 ? 16 : 24
            }
        }
        
        //内容区域
        Rectangle {
            id: bluetooth_content
            width: parent.width * 1274/1334
            height: aspectRatio > 1.7 ? parent.height * 62/75 : parent.height * 66/75
            x: parent.width * 30/1334
            y: aspectRatio > 1.7 ? parent.height * 10/75 : parent.height * 7/75
            color: "transparent"
            
            // 共享变量：速度值（只读，自动计算）
            property real speed: btn_speed.speedValue
            
            // 共享变量：命令字符串（各按钮在使用时设置）
            property string command: ""
            
            // Target 模式状态
            property bool isTargetMode: false
            
            // 共享的按钮配置参数
            property int quickTapSteps: 10  // 快速点按移动的步数（所有按钮共享）
            property int longPressThreshold: 300  // 长按判定时间阈值（毫秒，所有按钮共享）
            
            // 设置背景图
            Image {
                source: "qrc:/img/Set_bg.png"
                anchors.fill: parent
                fillMode: Image.Stretch
            }
            
            // 左上角状态指示器
            Rectangle {
                id: status_indicator
                width: aspectRatio > 1.7 ? parent.width * 0.04 : parent.width * 0.05
                height: width
                radius: width / 2
                anchors {
                    top: parent.top
                    left: parent.left
                    topMargin: parent.height * 0.02
                    leftMargin: parent.width * 0.02
                }
                color: bluetoothScanner.isConnected ? "#00FF00" : "#FF0000"
                
                // 中心白点
                Rectangle {
                    width: parent.width * 0.4
                    height: width
                    radius: width / 2
                    anchors.centerIn: parent
                    color: "white"
                }
            }
            
            // 右上角关闭按钮
            Button {
                id: btn_bluetooth_close
                width: aspectRatio > 1.7 ? parent.width * 0.05 : parent.width * 0.06//aspectRatio > 1.7 ? parent.width * 0.06 : parent.width * 0.07
                height: width
                padding: 0
                anchors {
                    top: parent.top
                    right: parent.right
                    topMargin: parent.height * 0.02
                    rightMargin: parent.width * 0.02
                }
                background: Rectangle {
                    color: "#333333"
                    radius: width / 2
                }
                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: "white"
                    font.pixelSize: parent.width * 0.5
                    font.bold: true
                }
                onClicked: {
                    btn_bluetooth_back.clicked()
                }

                //     if (bluetoothScanner.isConnected) {
                //         bluetoothScanner.disconnectDevice();
                //     }
                //     bluetoothControlDialog.visible = false;
                //     scanDialog.visible = true;
                // }
            }
            
            // 方向控制区域
            // RA+ 按钮 (上)
            Button {
                id: btn_ra_plus
                width: bluetooth_content.width*0.88/7//buttonWidth
                height: width/2//parent.width * 0.18
                padding: 0
                x:parent.width * 0.04+btn_ra_plus.width//bluetooth_content.width/8*2
                y:parent.height * 0.02//aspectRatio > 1.7 ? bluetooth_content.height/5:bluetooth_content.height/8
                enabled: !bluetooth_content.isTargetMode
                
                // 短按/长按检测
                property bool isLongPress: false
                property var pressTime: 0  // 使用 var 类型以支持 Date.now() 的大数值
                
                background: Rectangle {
                    color: !btn_ra_plus.enabled ? "#222222" : (btn_ra_plus.pressed ? "#555555" : "#444444")
                    radius: 15
                    border.color: !btn_ra_plus.enabled ? "#333333" : "#666666"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "RA+"
                    color: btn_ra_plus.enabled ? "white" : "#666666"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.45
                    font.bold: true
                }
                
                // 长按检测定时器
                Timer {
                    id: btn_ra_plus_longPressTimer
                    interval: bluetooth_content.longPressThreshold
                    repeat: false
                    onTriggered: {
                        btn_ra_plus.isLongPress = true;
                        // 长按触发时，发送 Tup 命令
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command = "Tup:" + bluetooth_content.speed.toFixed(4);
                            
                            if (typeof sendingAnimation !== 'undefined') {
                                sendingAnimation.start();
                            }
                            
                            appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("RA+ 长按触发，发送Tup命令");
                        }
                    }
                }
                
                // 点按时延迟发送停止命令的定时器
                Timer {
                    id: btn_ra_plus_tapStopTimer
                    interval: 500  // 500ms 延迟
                    repeat: false
                    onTriggered: {
                        if (bluetoothScanner.isConnected) {
                            var command = "stop_ud";
                            appWindow.addCommandLog(command + " (点按停止)", "sent");
                            bluetoothScanner.sendCommand(command);
                            console.log("RA+ 点按停止命令已发送");
                        }
                    }
                }
                
                onPressed: {
                    // 总是设置 pressTime，不管连接状态如何
                    pressTime = Date.now();
                    isLongPress = false;
                    console.log("RA+ onPressed: pressTime = " + pressTime);
                    
                    if (bluetoothScanner.isConnected) {
                        // 启动长按检测定时器，但不立即发送命令
                        // 只有长按时才会发送 Tup 命令
                        btn_ra_plus_longPressTimer.start();
                    }
                }
                
                onReleased: {
                    if (bluetoothScanner.isConnected) {
                        btn_ra_plus_longPressTimer.stop();
                        
                        var currentTime = Date.now();
                        console.log("RA+ onReleased: pressTime = " + pressTime + ", currentTime = " + currentTime);
                        
                        // 如果 pressTime 为 0 或未设置，说明 onPressed 没有被调用或没有正确设置，直接判断为短按
                        // 或者如果 pressDuration 异常大（> 10000ms），也判断为短按（防止初始值问题）
                        if (pressTime === 0 || pressTime === undefined || pressTime === null) {
                            // 点按：发送 Tup 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tup:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_ra_plus_tapStopTimer.start();
                            console.log("RA+ 快速点按（pressTime未设置），已发送Tup命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        var pressDuration = currentTime - pressTime;
                        console.log("RA+ onReleased: pressDuration = " + pressDuration + "ms, isLongPress = " + isLongPress);
                        
                        // 如果 isLongPress 为 true（定时器已触发），直接判断为长按
                        if (isLongPress) {
                            // 长按：发送停止命令（Tup:50已在长按定时器触发时发送）
                            bluetooth_content.command= "stop_ud";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("RA+ 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration 异常大（> 10000ms），说明 pressTime 可能有问题，判断为短按
                        if (pressDuration > 10000) {
                            console.log("RA+ pressDuration异常大，判断为短按");
                            // 点按：发送 Tup 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tup:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_ra_plus_tapStopTimer.start();
                            console.log("RA+ 快速点按（异常持续时间），已发送Tup命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration >= longPressThreshold，判断为长按
                        if (pressDuration >= bluetooth_content.longPressThreshold) {
                            // 长按：发送停止命令（如果长按定时器已触发，Tup:50已发送；否则需要先发送Tup:50）
                            if (!isLongPress) {
                                // 如果长按定时器还没触发，但持续时间已达到阈值，先发送Tup命令
                                bluetooth_content.command = "Tup:" + bluetooth_content.speed.toFixed(4);
                                appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                                bluetoothScanner.sendCommand(bluetooth_content.command);
                            }
                            bluetooth_content.command = "stop_ud";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("RA+ 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                        } else {
                            // 点按：发送 Tup 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tup:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_ra_plus_tapStopTimer.start();
                            console.log("RA+ 快速点按，已发送Tup命令，150ms后发送停止命令，持续时间: " + pressDuration + "ms");
                        }
                        
                        // 重置 pressTime
                        pressTime = 0;
                    }
                }
            }

            // RA- 按钮 (左)
            Button {
                id: btn_ra_minus
                width: btn_ra_plus.width
                height:width/2
                padding: 0
                x:parent.width * 0.02//bluetooth_content.width/8//7*2
                y:btn_ra_plus.y+btn_ra_plus.height+parent.height*0.01//aspectRatio > 1.7 ? bluetooth_content.height/5:bluetooth_content.height/8//btn_ra_plus.y
                enabled: !bluetooth_content.isTargetMode
                
                // 短按/长按检测
                property bool isLongPress: false
                property var pressTime: 0  // 使用 var 类型以支持 Date.now() 的大数值
                
                background: Rectangle {
                    color: !btn_ra_minus.enabled ? "#222222" : (btn_ra_minus.pressed ? "#555555" : "#444444")
                    radius: 15
                    border.color: !btn_ra_minus.enabled ? "#333333" : "#666666"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "RA-"
                    color: btn_ra_minus.enabled ? "white" : "#666666"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.45
                    font.bold: true
                }
                
                Timer {
                    id: btn_ra_minus_longPressTimer
                    interval: bluetooth_content.longPressThreshold
                    repeat: false
                    onTriggered: {
                        btn_ra_minus.isLongPress = true;
                        // 长按触发时，发送 Tleft 命令
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command = "Tleft:" + bluetooth_content.speed.toFixed(4);
                            
                            if (typeof sendingAnimation !== 'undefined') {
                                sendingAnimation.start();
                            }
                            
                            appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("RA- 长按触发，发送Tleft命令");
                        }
                    }
                }
                
                // 点按时延迟发送停止命令的定时器
                Timer {
                    id: btn_ra_minus_tapStopTimer
                    interval: 500  // 500ms 延迟
                    repeat: false
                    onTriggered: {
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command= "stop_rl";
                            appWindow.addCommandLog(bluetooth_content.command + " (点按停止)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("RA- 点按停止命令已发送");
                        }
                    }
                }
                
                onPressed: {
                    // 总是设置 pressTime，不管连接状态如何
                    pressTime = Date.now();
                    isLongPress = false;
                    console.log("RA- onPressed: pressTime = " + pressTime);
                    
                    if (bluetoothScanner.isConnected) {
                        // 启动长按检测定时器，但不立即发送命令
                        // 只有长按时才会发送 Tleft 命令
                        btn_ra_minus_longPressTimer.start();
                    }
                }
                
                onReleased: {
                    if (bluetoothScanner.isConnected) {
                        btn_ra_minus_longPressTimer.stop();
                        
                        var currentTime = Date.now();
                        console.log("RA- onReleased: pressTime = " + pressTime + ", currentTime = " + currentTime);
                        
                        // 如果 pressTime 为 0 或未设置，说明 onPressed 没有被调用或没有正确设置，直接判断为短按
                        // 或者如果 pressDuration 异常大（> 10000ms），也判断为短按（防止初始值问题）
                        if (pressTime === 0 || pressTime === undefined || pressTime === null) {
                            // 点按：发送 Tleft 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tleft:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_ra_minus_tapStopTimer.start();
                            console.log("RA- 快速点按（pressTime未设置），已发送Tleft命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        var pressDuration = currentTime - pressTime;
                        console.log("RA- onReleased: pressDuration = " + pressDuration + "ms, isLongPress = " + isLongPress);
                        
                        // 如果 isLongPress 为 true（定时器已触发），直接判断为长按
                        if (isLongPress) {
                            // 长按：发送停止命令（Tleft:50已在长按定时器触发时发送）
                            bluetooth_content.command = "stop_rl";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("RA- 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration 异常大（> 10000ms），说明 pressTime 可能有问题，判断为短按
                        if (pressDuration > 10000) {
                            console.log("RA- pressDuration异常大，判断为短按");
                            // 点按：发送 Tleft 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tleft:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_ra_minus_tapStopTimer.start();
                            console.log("RA- 快速点按（异常持续时间），已发送Tleft命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration >= longPressThreshold，判断为长按
                        if (pressDuration >= bluetooth_content.longPressThreshold) {
                            // 长按：发送停止命令（如果长按定时器已触发，Tleft:50已发送；否则需要先发送Tleft:50）
                            if (!isLongPress) {
                                // 如果长按定时器还没触发，但持续时间已达到阈值，先发送Tleft命令
                                bluetooth_content.command = "Tleft:" + bluetooth_content.speed.toFixed(4);
                                appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                                bluetoothScanner.sendCommand(bluetooth_content.command);
                            }
                            bluetooth_content.command= "stop_rl";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("RA- 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                        } else {
                            // 点按：发送 Tleft 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tleft:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_ra_minus_tapStopTimer.start();
                            console.log("RA- 快速点按，已发送Tleft命令，150ms后发送停止命令，持续时间: " + pressDuration + "ms");
                        }
                        
                        // 重置 pressTime
                        pressTime = 0;
                    }
                }
            }

            // 中心圆形停止按钮
            Button {
                id: btn_stop
                width: btn_ra_plus.width
                height: width/2
                padding: 0
                x:btn_ra_minus.x+btn_ra_minus.width+parent.width * 0.02//bluetooth_content.width/8*2//7*3
                y:btn_ra_minus.y
                background: Rectangle {
                    id: stopButtonBackground
                    color: bluetoothControlDialog.emergencyStopInProgress ? "#FF6600" : "#FF0000"
                    radius: btn_stop.width / 2
                    border.color: bluetoothControlDialog.emergencyStopInProgress ? "#FFFF00" : "transparent"
                    border.width: bluetoothControlDialog.emergencyStopInProgress ? 3 : 0
                    
                    // 停止进行中的闪烁动画
                    SequentialAnimation {
                        running: bluetoothControlDialog.emergencyStopInProgress
                        loops: Animation.Infinite
                        PropertyAnimation {
                            target: stopButtonBackground
                            property: "color"
                            to: "#FF0000"
                            duration: 300
                        }
                        PropertyAnimation {
                            target: stopButtonBackground
                            property: "color"
                            to: "#FF6600"
                            duration: 300
                        }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    text: "■"
                    color: "white"
                    font.pixelSize:aspectRatio>1.7 ?16:24// parent.height * 0.45
                    font.bold: true
                }
                
                onClicked: {
                    if (bluetoothScanner.isConnected) {
                        // 调用紧急停止函数
                        bluetoothControlDialog.emergencyStop();
                    } else {
                        showTemporaryTip(currentLanguage === "zh" ? 
                            "设备未连接" : 
                            "Device not connected");
                    }
                }
            }

            // DEC+ 按钮 (下)
            Button {
                id: btn_dec_plus
                width: btn_ra_plus.width
                height: width/2
                padding: 0
                x:btn_stop.x//bluetooth_content.width/8*2//7*4
                y:btn_ra_minus.y+btn_ra_minus.height+parent.height*0.01//aspectRatio > 1.7 ? bluetooth_content.height/5+btn_ra_minus.y:bluetooth_content.height/8+btn_ra_minus.y///btn_ra_plus.y
                enabled: !bluetooth_content.isTargetMode
                
                // 短按/长按检测
                property bool isLongPress: false
                property var pressTime: 0  // 使用 var 类型以支持 Date.now() 的大数值
                
                background: Rectangle {
                    color: !btn_dec_plus.enabled ? "#222222" : (btn_dec_plus.pressed ? "#555555" : "#444444")
                    radius: 15
                    border.color: !btn_dec_plus.enabled ? "#333333" : "#666666"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "DEC+"
                    color: btn_dec_plus.enabled ? "white" : "#666666"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.45
                    font.bold: true
                }
                
                Timer {
                    id: btn_dec_plus_longPressTimer
                    interval: bluetooth_content.longPressThreshold
                    repeat: false
                    onTriggered: {
                        btn_dec_plus.isLongPress = true;
                        // 长按触发时，发送 Tdown 命令
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command = "Tdown:" + bluetooth_content.speed.toFixed(4);
                            
                            if (typeof sendingAnimation !== 'undefined') {
                                sendingAnimation.start();
                            }
                            
                            appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC+ 长按触发，发送Tdown命令");
                        }
                    }
                }
                
                // 点按时延迟发送停止命令的定时器
                Timer {
                    id: btn_dec_plus_tapStopTimer
                    interval: 500  // 500ms 延迟
                    repeat: false
                    onTriggered: {
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command= "stop_ud";
                            appWindow.addCommandLog(bluetooth_content.command + " (点按停止)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC+ 点按停止命令已发送");
                        }
                    }
                }
                
                onPressed: {
                    // 总是设置 pressTime，不管连接状态如何
                    pressTime = Date.now();
                    isLongPress = false;
                    console.log("DEC+ onPressed: pressTime = " + pressTime);
                    
                    if (bluetoothScanner.isConnected) {
                        // 启动长按检测定时器，但不立即发送命令
                        // 只有长按时才会发送 Tdown 命令
                        btn_dec_plus_longPressTimer.start();
                    }
                }
                
                onReleased: {
                    if (bluetoothScanner.isConnected) {
                        btn_dec_plus_longPressTimer.stop();
                        
                        var currentTime = Date.now();
                        console.log("DEC+ onReleased: pressTime = " + pressTime + ", currentTime = " + currentTime);
                        
                        // 如果 pressTime 为 0 或未设置，说明 onPressed 没有被调用或没有正确设置，直接判断为短按
                        // 或者如果 pressDuration 异常大（> 10000ms），也判断为短按（防止初始值问题）
                        if (pressTime === 0 || pressTime === undefined || pressTime === null) {
                            // 点按：发送 Tdown 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tdown:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_dec_plus_tapStopTimer.start();
                            console.log("DEC+ 快速点按（pressTime未设置），已发送Tdown命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        var pressDuration = currentTime - pressTime;
                        console.log("DEC+ onReleased: pressDuration = " + pressDuration + "ms, isLongPress = " + isLongPress);
                        
                        // 如果 isLongPress 为 true（定时器已触发），直接判断为长按
                        if (isLongPress) {
                            // 长按：发送停止命令（Tdown:50已在长按定时器触发时发送）
                            bluetooth_content.command = "stop_ud";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC+ 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration 异常大（> 10000ms），说明 pressTime 可能有问题，判断为短按
                        if (pressDuration > 10000) {
                            console.log("DEC+ pressDuration异常大，判断为短按");
                            // 点按：发送 Tdown 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tdown:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_dec_plus_tapStopTimer.start();
                            console.log("DEC+ 快速点按（异常持续时间），已发送Tdown命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration >= longPressThreshold，判断为长按
                        if (pressDuration >= bluetooth_content.longPressThreshold) {
                            // 长按：发送停止命令（如果长按定时器已触发，Tdown:50已发送；否则需要先发送Tdown:50）
                            if (!isLongPress) {
                                // 如果长按定时器还没触发，但持续时间已达到阈值，先发送Tdown命令
                                bluetooth_content.command = "Tdown:" + bluetooth_content.speed.toFixed(4);
                                appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                                bluetoothScanner.sendCommand(bluetooth_content.command);
                            }
                            bluetooth_content.command = "stop_ud";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC+ 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                        } else {
                            // 点按：发送 Tdown 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tdown:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_dec_plus_tapStopTimer.start();
                            console.log("DEC+ 快速点按，已发送Tdown命令，150ms后发送停止命令，持续时间: " + pressDuration + "ms");
                        }
                        
                        // 重置 pressTime
                        pressTime = 0;
                    }
                }
            }

            // DEC- 按钮 (右)
            Button {
                id: btn_dec_minus
                width:btn_ra_plus.width
                height: width/2
                padding: 0
                x:btn_stop.x+btn_stop.width+parent.width * 0.02//bluetooth_content.width/8*3//7*5
                y:btn_ra_minus.y
                enabled: !bluetooth_content.isTargetMode
                
                // 短按/长按检测
                property bool isLongPress: false
                property var pressTime: 0  // 使用 var 类型以支持 Date.now() 的大数值
                
                background: Rectangle {
                    color: !btn_dec_minus.enabled ? "#222222" : (btn_dec_minus.pressed ? "#555555" : "#444444")
                    radius: 15
                    border.color: !btn_dec_minus.enabled ? "#333333" : "#666666"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "DEC-"
                    color: btn_dec_minus.enabled ? "white" : "#666666"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.45
                    font.bold: true
                }
                
                Timer {
                    id: btn_dec_minus_longPressTimer
                    interval: bluetooth_content.longPressThreshold
                    repeat: false
                    onTriggered: {
                        btn_dec_minus.isLongPress = true;
                        // 长按触发时，发送 Tright 命令
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command = "Tright:" + bluetooth_content.speed.toFixed(4);
                            
                            if (typeof sendingAnimation !== 'undefined') {
                                sendingAnimation.start();
                            }
                            
                            appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC- 长按触发，发送Tright命令");
                        }
                    }
                }
                
                // 点按时延迟发送停止命令的定时器
                Timer {
                    id: btn_dec_minus_tapStopTimer
                    interval: 500  //500ms 延迟
                    repeat: false
                    onTriggered: {
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command = "stop_rl";
                            appWindow.addCommandLog(bluetooth_content.command + " (点按停止)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC- 点按停止命令已发送");
                        }
                    }
                }
                
                onPressed: {
                    // 总是设置 pressTime，不管连接状态如何
                    pressTime = Date.now();
                    isLongPress = false;
                    console.log("DEC- onPressed: pressTime = " + pressTime);
                    
                    if (bluetoothScanner.isConnected) {
                        // 启动长按检测定时器，但不立即发送命令
                        // 只有长按时才会发送 Tright 命令
                        btn_dec_minus_longPressTimer.start();
                    }
                }
                
                onReleased: {
                    if (bluetoothScanner.isConnected) {
                        btn_dec_minus_longPressTimer.stop();
                        
                        var currentTime = Date.now();
                        console.log("DEC- onReleased: pressTime = " + pressTime + ", currentTime = " + currentTime);
                        
                        // 如果 pressTime 为 0 或未设置，说明 onPressed 没有被调用或没有正确设置，直接判断为短按
                        // 或者如果 pressDuration 异常大（> 10000ms），也判断为短按（防止初始值问题）
                        if (pressTime === 0 || pressTime === undefined || pressTime === null) {
                            // 点按：发送 Tright 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tright:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_dec_minus_tapStopTimer.start();
                            console.log("DEC- 快速点按（pressTime未设置），已发送Tright命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        var pressDuration = currentTime - pressTime;
                        console.log("DEC- onReleased: pressDuration = " + pressDuration + "ms, isLongPress = " + isLongPress);
                        
                        // 如果 isLongPress 为 true（定时器已触发），直接判断为长按
                        if (isLongPress) {
                            // 长按：发送停止命令（Tright:50已在长按定时器触发时发送）
                            bluetooth_content.command = "stop_rl";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC- 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration 异常大（> 10000ms），说明 pressTime 可能有问题，判断为短按
                        if (pressDuration > 10000) {
                            console.log("DEC- pressDuration异常大，判断为短按");
                            // 点按：发送 Tright 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tright:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_dec_minus_tapStopTimer.start();
                            console.log("DEC- 快速点按（异常持续时间），已发送Tright命令，150ms后发送停止命令");
                            pressTime = 0; // 重置
                            return;
                        }
                        
                        // 如果 pressDuration >= longPressThreshold，判断为长按
                        if (pressDuration >= bluetooth_content.longPressThreshold) {
                            // 长按：发送停止命令（如果长按定时器已触发，Tright:50已发送；否则需要先发送Tright:50）
                            if (!isLongPress) {
                                // 如果长按定时器还没触发，但持续时间已达到阈值，先发送Tright命令
                                bluetooth_content.command = "Tright:" + bluetooth_content.speed.toFixed(4);
                                appWindow.addCommandLog(bluetooth_content.command + " (长按)", "sent");
                                bluetoothScanner.sendCommand(bluetooth_content.command);
                            }
                            bluetooth_content.command= "stop_rl";
                            appWindow.addCommandLog(bluetooth_content.command + " (长按松开)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            console.log("DEC- 长按松开，停止运动，持续时间: " + pressDuration + "ms");
                        } else {
                            // 点按：发送 Tright 命令，然后150ms后发送停止命令
                            bluetooth_content.command = "Tright:" + bluetooth_content.speed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command + " (点按)", "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            // 启动延迟停止定时器
                            btn_dec_minus_tapStopTimer.start();
                            console.log("DEC- 快速点按，已发送Tright命令，150ms后发送停止命令，持续时间: " + pressDuration + "ms");
                        }
                        
                        // 重置 pressTime
                        pressTime = 0;
                    }
                }
            }

            
            // 功能按钮（使用anchors定位：PARK、Target、Home、Refresh、Speed）
            // PARK 按钮
            Button {
                id: btn_park
                width: btn_ra_plus.width
                height: width/2
                padding: 0
                x:parent.width * 0.02//bluetooth_content.width/8
                y:btn_dec_plus.y+btn_dec_plus.height+parent.height*0.01//aspectRatio > 1.7 ? bluetooth_content.height/5+btn_dec_plus.y:bluetooth_content.height/8+btn_dec_plus.y
                property bool isParked: false
                background: Rectangle {
                    color: "#444444"
                    radius: 12
                    border.color: "#555555"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: btn_park.isParked ? "UNPARK" : "PARK"
                    color: "white"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.35
                    font.bold: true
                }
                onClicked: {
                    if (bluetoothScanner.isConnected) {
                        if (btn_park.isParked) {
                            appWindow.addCommandLog("UNPARK", "sent");
                            bluetoothScanner.sendCommand("UNPARK");
                        } else {
                            appWindow.addCommandLog("PARK", "sent");
                            bluetoothScanner.sendCommand("PARK");
                        }
                        btn_park.isParked = !btn_park.isParked
                    }
                }
            }
            
            // Target 按钮
            Button {
                id: btn_target
                width: btn_ra_plus.width
                height: width/2
                padding: 0
                x:btn_stop.x//bluetooth_content.width/8*2
                y:btn_park.y
                background: Rectangle {
                    color: bluetooth_content.isTargetMode ? "#666666" : "#444444"
                    radius: 12
                    border.color: bluetooth_content.isTargetMode ? "#888888" : "#555555"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "⊙"
                    color: "white"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.5
                    font.bold: true
                }
                onClicked: {
                    if (bluetoothScanner.isConnected) {
                        if (!bluetooth_content.isTargetMode) {
                            // 激活 Target 模式：禁用方向按钮，发送 Tup+速度 和 stop_rl
                            bluetooth_content.isTargetMode = true;
                            
                            // 固定速度值为 0.05926 * (2^0) = 0.05926，并同步更新速度按钮显示
                            btn_speed.speedLevel = 1;
                            var fixedSpeed = 0.05926;
                            bluetooth_content.command = "Tup:" + fixedSpeed.toFixed(4);
                            appWindow.addCommandLog(bluetooth_content.command, "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                            
                            // 发送 SPEED:1 命令同步设备
                            var speedCommand = "SPEED:1";
                            appWindow.addCommandLog(speedCommand, "sent");
                            bluetoothScanner.sendCommand(speedCommand);
                            
                            appWindow.addCommandLog("stop_rl", "sent");
                            bluetoothScanner.sendCommand("stop_rl");
                        } else {
                            // 取消 Target 模式：启用方向按钮，发送 stop_ud
                            bluetooth_content.isTargetMode = false;
                            appWindow.addCommandLog("stop_ud", "sent");
                            bluetoothScanner.sendCommand("stop_ud");
                        }
                    }
                }
            }
            
            // Home 按钮
            Button {
                id: btn_home
                width: btn_ra_plus.width
                height: width/2
                padding: 0
                x:btn_dec_minus.x//bluetooth_content.width/8*3
                y:btn_park.y
                background: Rectangle {
                    color: "#444444"
                    radius: 12
                    border.color: "#555555"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "⌂"
                    color: "white"
                    font.pixelSize:aspectRatio>1.7 ?20:28// parent.height * 0.5
                    font.bold: true
                }
                onClicked: {
                    if (bluetoothScanner.isConnected) {
                        appWindow.addCommandLog("HOME", "sent");
                        bluetoothScanner.sendCommand("HOME");
                    }
                }
            }
            
            // Refresh/Loop 按钮（同步经纬度）
            Button {
                id: btn_refresh
                width: btn_ra_plus.width
                height: width/2
                padding: 0
                x:parent.width * 0.02//bluetooth_content.width/8
                y:btn_park.y+btn_park.height+parent.height*0.01//aspectRatio > 1.7 ? bluetooth_content.height/5+btn_park.y:bluetooth_content.height/8+btn_park.y//btn_park.y
                background: Rectangle {
                    color: "#444444"
                    radius: 12
                    border.color: "#555555"
                    border.width: 1
                }
                Text {
                    anchors.centerIn: parent
                    text: "同步"
                    color: "white"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.5
                    font.bold: true
                }
                onClicked: {
                    if (bluetoothScanner.isConnected) {
                        // 同步当前解析的经纬度到蓝牙设备
                        var lat = currentLat !== "0" ? currentLat : "0";
                        var lon = currentLong !== "0" ? currentLong : "0";
                        var syncCommand = "SYNC:" + lat + "," + lon;
                        console.log("Sending sync command:", syncCommand);
                        appWindow.addCommandLog(syncCommand, "sent");
                        bluetoothScanner.sendCommand(syncCommand);
                    }
                }
            }
            
            // Speed 指针盘（长条弧形）
            Rectangle {
                id: btn_speed
                width: btn_ra_plus.width * 2+parent.width*0.02
                height: btn_ra_plus.height *1.6 // 直接使用 width，因为 btn_ra_plus.height = width/2，所以 width = height*2
                x:btn_stop.x//bluetooth_content.width/8*2
                y:btn_park.y+btn_park.height+parent.height*0.01//btn_refresh.y//btn_ra_minus.y//
                property int speedLevel: 1  // 1-11
                property real speedValue: 0.05926 * Math.pow(2, speedLevel - 1)  // 速度值计算：0.05926 × (2^(指针值-1))
                color: "#444444"
                radius: 12
                border.color: "#555555"
                border.width: 1

                
                // 可点击区域
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        // 点击一下增加1，循环1-11
                        btn_speed.speedLevel = (btn_speed.speedLevel % 11) + 1;                        
                        if (bluetoothScanner.isConnected) {
                            bluetooth_content.command = "SPEED:" + btn_speed.speedLevel;
                            appWindow.addCommandLog(bluetooth_content.command, "sent");
                            bluetoothScanner.sendCommand(bluetooth_content.command);
                        }
                    }
                }
                
                // 弧形刻度盘（在文本上方）
                Canvas {
                    id: speed_dial_canvas
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        bottom: speed_text.top
                        margins: 4
                    }
                    
                    property real centerX: width / 2
                    property real centerY: height*0.95 // 圆心在底部下方，形成上半圆
                    property real radius: height * 0.78  // 半径
                    property real startAngle: Math.PI  // 180度（左侧）
                    property real endAngle: 0          // 0度（右侧）
                    property real angleRange: Math.PI // 180度范围（上半圆）
                    
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        
                        // 绘制上半圆弧形背景（顺时针从左到右）
                        ctx.beginPath();
                        // 从180度顺时针到0度（360度），显示上半圆
                        //ctx.arc(centerX, centerY, radius, startAngle, endAngle, false);
                        ctx.arc(centerX, centerY, radius, Math.PI, 2*Math.PI, false);
                        ctx.lineWidth = 2;
                        ctx.strokeStyle = "#666666";
                        ctx.stroke();
                        
                        // 绘制刻度线和数字
                        ctx.strokeStyle = "#888888";
                        ctx.fillStyle = "white";
                        ctx.font = (aspectRatio>1.7 ? 8 : 10) + "px Arial";
                        ctx.textAlign = "center";
                        ctx.textBaseline = "middle";
                        
                        for (var i = 1; i <= 11; i++) {
                            // 从左（180度）顺时针到右（0度），i从1到11递增
                            // 确保角度在0到2π范围内
                            var t = (i - 1) / 10;
                            var angle = startAngle + t * angleRange;           // π -> 2π (上半圆)
                            if (angle < 0) angle = angle + 2 * Math.PI;

                            var cos = Math.cos(angle);
                            var sin = Math.sin(angle);
                            
                            // 刻度线起点和终点
                            var innerRadius = radius * 0.85;
                            var outerRadius = radius;
                            
                            if (i === 1 || i === 6 || i === 11) {
                                // 长刻度线（1, 6, 11）
                                outerRadius = radius * 1.08;
                                ctx.lineWidth = 2;
                            } else {
                                ctx.lineWidth = 1;
                            }
                            
                            var x1 = centerX + innerRadius * cos;
                            var y1 = centerY + innerRadius * sin;
                            var x2 = centerX + outerRadius * cos;
                            var y2 = centerY + outerRadius * sin;
                            
                            ctx.beginPath();
                            ctx.moveTo(x1, y1);
                            ctx.lineTo(x2, y2);
                            ctx.stroke();
                            
                            // 绘制数字（在主要刻度处显示：1, 3, 5, 7, 9, 11）
                            if (i === 1 || i === 3 || i === 5 || i === 7 || i === 9 || i === 11) {
                                var textRadius = radius * 1.15;
                                var textX = centerX + textRadius * cos;
                                var textY = centerY + textRadius * sin;
                                ctx.fillText(i.toString(), textX, textY);
                            }
                        }
                        
                        // 绘制指针（沿着弧形，从左顺时针到右）
                        var pt = (btn_speed.speedLevel - 1) / 10;
                        var pointerAngle = startAngle + pt * angleRange;   // π -> 2π (上半圆)
                        // 确保角度在0到2π范围内
                        if (pointerAngle < 0) pointerAngle = pointerAngle + 2 * Math.PI;
                        var pointerCos = Math.cos(pointerAngle);
                        var pointerSin = Math.sin(pointerAngle);
                        var pointerRadius = radius * 0.9;
                        var pointerX = centerX + pointerRadius * pointerCos;
                        var pointerY = centerY + pointerRadius * pointerSin;
                        
                        // 绘制指针圆点
                        ctx.beginPath();
                        ctx.arc(pointerX, pointerY, 3, 0, 2 * Math.PI);
                        ctx.fillStyle = "#00FF00";
                        ctx.fill();
                        
                        // 绘制指针线（从弧形指向外）
                        ctx.beginPath();
                        ctx.moveTo(pointerX, pointerY);
                        var pointerOuterX = centerX + (radius * 1.12) * pointerCos;
                        var pointerOuterY = centerY + (radius * 1.12) * pointerSin;
                        ctx.lineTo(pointerOuterX, pointerOuterY);
                        ctx.lineWidth = 2;
                        ctx.strokeStyle = "#00FF00";
                        ctx.stroke();
                    }
                    
                    // 当speedLevel改变时重新绘制
                    Connections {
                        target: btn_speed
                        function onSpeedLevelChanged() {
                            speed_dial_canvas.requestPaint();
                        }
                    }
                    
                    Component.onCompleted: requestPaint()
                }
                
                // 速度值显示
                Text {
                    id: speed_text
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    text: "SPEED: " + btn_speed.speedLevel + " (" + btn_speed.speedValue.toFixed(4) + ")"
                    color: "white"
                    font.pixelSize: aspectRatio>1.7 ? 10 : 14
                    font.bold: true
                }
            }
            
            // 命令状态显示区域
            Rectangle {
                id: command_status_area
                width: parent.width * 0.5
                height: parent.height * 0.8//aspectRatio > 1.7 ? parent.height * 0.3 : parent.height * 0.25
                x: btn_dec_minus.x+btn_dec_minus.width+parent.width * 0.05
                y: btn_bluetooth_close.y + btn_bluetooth_close.height + parent.height * 0.02
                color: "#2a2a2a"
                radius: 10
                border.color: "#555555"
                border.width: 2
                
                // 标题
                Text {
                    id: status_title
                    text: currentLanguage === "zh" ? "命令日志" : "Command Log"
                    color: "#00FF00"
                    font.pixelSize: aspectRatio>1.7 ?16:24//parent.height * 0.08
                    font.bold: true
                    anchors {
                        top: parent.top
                        left: parent.left
                        topMargin: parent.height * 0.03
                        leftMargin: parent.width * 0.03
                    }
                }
                
                // 发送指示器
                Rectangle {
                    id: send_indicator
                    width: parent.height * 0.06
                    height: width
                    radius: width / 2
                    anchors {
                        top: parent.top
                        right: parent.right
                        topMargin: parent.height * 0.03
                        rightMargin: parent.width * 0.03
                    }
                    color: "#555555"
                    
                    // 发送动画
                    SequentialAnimation {
                        id: sendingAnimation
                        loops: 1
                        PropertyAnimation {
                            target: send_indicator
                            property: "color"
                            to: "#FFFF00"  // 黄色表示发送中
                            duration: 100
                        }
                        PropertyAnimation {
                            target: send_indicator
                            property: "color"
                            to: "#555555"
                            duration: 100
                        }
                    }
                    
                    // 接收动画
                    SequentialAnimation {
                        id: receiveAnimation
                        loops: 1
                        PropertyAnimation {
                            target: send_indicator
                            property: "color"
                            to: "#00FF00"  // 绿色表示接收成功
                            duration: 150
                        }
                        PauseAnimation {
                            duration: 500
                        }
                        PropertyAnimation {
                            target: send_indicator
                            property: "color"
                            to: "#555555"
                            duration: 150
                        }
                    }
                }
                
                // 命令历史列表
                ListView {
                    id: command_history_list
                    width: parent.width * 0.94
                    height: parent.height * 0.7
                    anchors {
                        top: status_title.bottom
                        topMargin: parent.height * 0.05
                        horizontalCenter: parent.horizontalCenter
                    }
                    clip: true
                    
                    model: ListModel {
                        id: commandLogModel
                    }
                    
                    delegate: Rectangle {
                        width: command_history_list.width
                        height: 25
                        color: "transparent"
                        
                        Row {
                            anchors.fill: parent
                            spacing: 8
                            
                            Text {
                                text: model.timestamp
                                color: "#888888"
                                font.pixelSize: 10
                                width: parent.width * 0.15
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            Text {
                                text: model.type === "sent" ? "→" : "←"
                                color: model.type === "sent" ? "#FFAA00" : "#00FF00"
                                font.pixelSize: 14
                                font.bold: true
                                width: parent.width * 0.05
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            Text {
                                text: model.command
                                color: model.type === "sent" ? "#FFFFFF" : "#00FF00"
                                font.pixelSize: 11
                                width: parent.width * 0.7
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    
                    // 自动滚动到底部的定时器
                    Timer {
                        id: scrollToBottomTimer
                        interval: 10  // 10ms 延迟，确保视图已更新
                        onTriggered: {
                            if (commandLogModel.count > 0) {
                                var lastIndex = commandLogModel.count - 1
                                command_history_list.positionViewAtIndex(lastIndex, ListView.End)
                                // 确保滚动位置正确，如果还不够，再尝试一次
                                Qt.callLater(function() {
                                    if (command_history_list.contentY + command_history_list.height < command_history_list.contentHeight) {
                                        command_history_list.positionViewAtIndex(lastIndex, ListView.End)
                                    }
                                })
                            }
                        }
                    }
                    
                    // 自动滚动到底部 - 监听 ListView 的 count 变化
                    onCountChanged: {
                        scrollToBottomTimer.restart()
                    }
                }
                
                // 清空按钮
                Button {
                    id: btn_clear_log
                    width: parent.width * 0.15
                    height: parent.height * 0.15
                    anchors {
                        bottom: parent.bottom
                        right: parent.right
                        bottomMargin: parent.height * 0.02
                        rightMargin: parent.width * 0.02
                    }
                    background: Rectangle {
                        color: btn_clear_log.pressed ? "#444444" : "#333333"
                        radius: 5
                        border.color: "#555555"
                        border.width: 1
                    }
                    Text {
                        anchors.centerIn: parent
                        text: currentLanguage === "zh" ? "清空" : "Clear"
                        color: "white"
                        font.pixelSize: parent.height * 0.4
                    }
                    onClicked: {
                        commandLogModel.clear()
                    }
                }
            }

        }
        
        // 监听连接状态变化，更新显示
        Connections {
            target: bluetoothScanner
            
            function onConnectionStateChanged(connected) {
                // 如果断开连接，返回扫描页面
                if (!connected && bluetoothControlDialog.visible) {
                    bluetoothControlDialog.visible = false
                    scanDialog.visible = true
                }
            }
            
            function onDataReceived(data) {
                // 调用主窗口的处理函数
                onDataReceived(data)
            }
            
            function onConnectionError(error) {
                appWindow.addCommandLog("Error: " + error, "received");
            }
        }
    }

    //设置页面
    Rectangle {
        id: moreDialog
        visible: false
        anchors.fill: parent
        color:  "transparent"
        property int mainPageIndex: 0
        property int helpPageIndex: 1
        property int aboutPageIndex: 2
        //property int updatePageIndex: 3
        property int agreementPageIndex: 3//4
        property int policyPageIndex: 4//5
        //标题栏
        Rectangle {//1334*70  (0,0)
            id: set_title
            width:parent.width
            height:aspectRatio>1.7?parent.height*7/75:parent.height*5/75
            x:0
            y:0
            color:  "transparent"
            Image {
                id:set_bg_title
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/Scan_title.png"
                opacity:1
            }
            //返回按钮
            Button{//17*30   (30,20)
                id:btn_set_back
                width: parent.width*47/1334
                height: parent.height
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                }
                background: Rectangle {
                    color: "transparent" // 背景色设置为透明
                }
                Image {
                    id: btn_Img_set_back
                    source: "qrc:/img/Btn_back.png"
                    width: title.width*0.013//30 // 图标的大小
                    height: title.height*3/7//30
                    anchors {
                        right: parent.right // 图标靠右
                        verticalCenter: parent.verticalCenter // 图标垂直居中//bottom: parent.bottom // 图标靠下
                        margins: 5 // 适当设置边距，使图标不紧贴按钮边缘
                    }
                }
                onClicked: {
                    //downloadProgress.value=0
                    //progressText.text=""
                    //downloadStatusText.text=""
                    //progressText.visible=false
                    //downloadStatusText.visible=false
                    if(stackLayout_more.currentIndex === moreDialog.helpPageIndex ||
                            stackLayout_more.currentIndex === moreDialog.aboutPageIndex ||
                            stackLayout_more.currentIndex === moreDialog.agreementPageIndex ||
                            stackLayout_more.currentIndex === moreDialog.policyPageIndex){
                        // 返回主页面
                        stackLayout_more.currentIndex = moreDialog.mainPageIndex
                        set_title_text.text = currentLanguage === "zh" ? qsTr("设置") : qsTr("Settings")
                    }/*
                    // else if(stackLayout_more.currentIndex === moreDialog.updatePageIndex) {
                    //     // 从更新页面返回到关于页面
                    //     stackLayout_more.currentIndex = moreDialog.aboutPageIndex
                    //     set_title_text.text = currentLanguage === "zh" ? qsTr("关于") : qsTr("About")
                    // }*/
                    else {
                        // 从主设置页面返回
                        moreDialog.visible = false
                        startDialog.visible = true
                    }
                }
            }
            //标题文字
            Text {  //anchors.verticalCenter: parent.verticalCenter //垂直居中
                id:set_title_text
                anchors.centerIn: parent//anchors.fill: parent     //x: parent.height*0.7
                text: currentLanguage === "zh" ? qsTr("设置"):qsTr("Settings")
                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                color: "white"
                font.pixelSize:aspectRatio>1.7 ?16:24// aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20
            }
        }
        //内容栏
        Rectangle {//1274*620  (30,100)
            id:set_connect
            width: parent.width*1274/1334
            height: aspectRatio>1.7?parent.height*62/75:parent.height*66/75
            x: parent.width*30/1334
            y:aspectRatio>1.7?parent.height*10/75:parent.height*7/75
            color:  "transparent"
            // 设置背景图
            Image {
                source: "qrc:/img/Set_bg.png"
                anchors.fill: parent
                fillMode: Image.Stretch // 拉伸模式//opacity:1
            }
            StackLayout{
                id: stackLayout_more
                anchors.fill: parent
                currentIndex: 0
                //设置主页内容0
                Rectangle{
                    id: mainPage
                    color: "transparent"
                    //设置区域
                    Rectangle {//(1210*520)  (60,130)---(30,30)
                        id:set_main
                        width: set_connect.width *1210/ 1274
                        height: set_connect.height*52 / 62
                        x: set_connect.width*30/ 1274
                        y: set_connect.height*3/62
                        color: "transparent"
                        Image {
                            source: "qrc:/img/Set_bg_main.png"
                            anchors.fill: parent
                            fillMode: Image.Stretch // 拉伸模式
                        }
                        //语言设置图标
                        Rectangle {//100*103  //50*50  (90,157)---(30,27)
                            id:set_languageIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                            width: parent.width*10/121
                            height: parent.height*103/520
                            anchors{
                                top:parent.top
                                left: parent.left
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_lanIcon.png";
                                fillMode: Image.PreserveAspectFit
                                width: set_languageIcon.width *5/8
                                height: set_languageIcon.height *50/103//set_languageIcon.height *23/50//anchors.centerIn:ipIcon// parent
                                anchors.centerIn: set_languageIcon
                            }
                        }
                        //语言设置文字
                        Text {
                            text:currentLanguage === "zh" ? qsTr("语言设置"): qsTr("Language Settings")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            color: "white"
                            font.pixelSize:aspectRatio>1.7 ?16:24// aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 //anchors.centerIn: parent
                            x:set_languageIcon.width
                            anchors.verticalCenter: set_languageIcon.verticalCenter //垂直居中
                        }
                        //语言切换
                        ComboBox {
                            width: set_languageIcon.width*2
                            height: set_languageIcon.height
                            anchors{
                                top:parent.top
                                right: parent.right
                                rightMargin: set_languageIcon.width*3/8
                            }
                            // 自定义边框
                            background: Rectangle {
                                anchors.fill: parent
                                color: "transparent"  // 背景透明
                            }
                            model: ["English", "中文"]
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02
                            popup.font.pixelSize: aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02
                            popup.font.family:  (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                             // 使用 Component.onCompleted 来设置 currentIndex
                            Component.onCompleted: {
                                currentIndex = (currentLanguage === "en") ? 0 : 1;
                            }
                            onCurrentTextChanged: { // 语言切换逻辑
                                currentLanguage = currentIndex === 0 ? "en" : "zh";
                                languageSettings.language =currentLanguage
                                saveLanguageSettings()
                                // changeLanguage(currentLanguage)
                                set_title_text.text = currentLanguage === "zh" ? qsTr("设置") : qsTr("Settings");
                            }
                            // 自定义下拉框箭头图标
                            indicator: Item {
                                height:parent.height*0.1//10/103
                                anchors.verticalCenter: parent.verticalCenter //垂直居中
                                anchors.right: parent.right      //anchors.rightMargin: set_languageIcon.width*3/8  // 设置右侧间距，确保箭头不超出边界
                                Image {
                                    height: parent.height//*0.1
                                    anchors.right:parent.right //anchors.verticalCenter: parent.verticalCenter //垂直居中  //anchors.rightMargin: set_languageIcon.width*3/8
                                    source: "qrc:/img/set_combox.png"  //自定义箭头图标
                                    fillMode: Image.PreserveAspectFit
                                }
                            }
                        }
                        //分割线01
                        Rectangle {
                            id:setLine01
                            width: parent.width-2-set_languageIcon.width *3/8
                            height: 1
                            anchors{
                                left:parent.left
                                right: parent.right
                                leftMargin: set_languageIcon.width
                                rightMargin: set_languageIcon.width *3/8
                            }
                            y: set_languageIcon.height
                            color: "transparent"
                            Image {
                                anchors.fill: parent
                                source: "qrc:/img/IP_line.png"
                                fillMode: Image.Stretch
                            }
                        }

                        //帮助图标
                        Rectangle {
                            id:set_helpIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_languageIcon.bottom
                                left: parent.left
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_helpIcon.png";
                                fillMode: Image.PreserveAspectFit
                                width: set_helpIcon.width *5/8
                                height: set_helpIcon.height *50/103
                                anchors.centerIn: set_helpIcon
                            }
                        }
                        //帮助文字
                        Text {
                            text:currentLanguage === "zh" ? qsTr("帮助"): qsTr("Help")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            color: "white"
                            font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 //anchors.centerIn: parent
                            x:set_helpIcon.width
                            anchors.verticalCenter: set_helpIcon.verticalCenter //垂直居中
                        }
                        //箭头图标
                        Rectangle {
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                right: parent.right
                                rightMargin: set_languageIcon.width *3/8
                                top:set_languageIcon.bottom
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_right.png"  //自定义箭头图标
                                height: parent.height*0.2
                                anchors.right:parent.right
                                anchors.verticalCenter: parent.verticalCenter //垂直居中
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        //分割线02
                        Rectangle {
                            id:setLine02
                            width: parent.width-2-set_languageIcon.width *3/8
                            height: 1
                            anchors{
                                left:parent.left
                                right: parent.right
                                leftMargin: set_helpIcon.width
                                rightMargin: set_languageIcon.width *3/8
                            }
                            y: set_helpIcon.y+set_helpIcon.height
                            color: "transparent"
                            Image {
                                anchors.fill: parent
                                source: "qrc:/img/IP_line.png"
                                fillMode: Image.Stretch
                            }
                        }
                        MouseArea {
                            width: set_main.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_languageIcon.bottom
                                left: parent.left
                            }
                            onClicked: {
                                // stackLayout_more.currentIndex = 1 // 跳转到帮助页面
                                // currentPage="help"
                                stackLayout_more.currentIndex = moreDialog.helpPageIndex
                                set_title_text.text=currentLanguage === "zh" ? qsTr("帮助中心"): qsTr("Help Center")
                            }
                        }

                        //关于图标
                        Rectangle {
                            id:set_aboutIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_helpIcon.bottom
                                left: parent.left
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_aboutIcon.png";
                                fillMode: Image.PreserveAspectFit
                                width: set_aboutIcon.width *5/8
                                height: set_aboutIcon.height *50/103//set_languageIcon.height *23/50//anchors.centerIn:ipIcon// parent
                                anchors.centerIn: set_aboutIcon
                            }
                        }
                        //关于文字
                        Text {
                            text:currentLanguage === "zh" ? qsTr("关于"): qsTr("About")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            color: "white"
                            font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 //anchors.centerIn: parent
                            x:set_aboutIcon.width
                            anchors.verticalCenter: set_aboutIcon.verticalCenter //垂直居中
                        }
                        //箭头图标
                        Rectangle {
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                right: parent.right
                                rightMargin: set_languageIcon.width *3/8
                                top:set_helpIcon.bottom
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_right.png"  //自定义箭头图标
                                height: parent.height*0.2
                                anchors.right:parent.right
                                anchors.verticalCenter: parent.verticalCenter //垂直居中
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        //分割线03
                        Rectangle {
                            id:setLine03
                            width: parent.width-2-set_languageIcon.width *3/8
                            height: 1
                            anchors{
                                left:parent.left
                                right: parent.right
                                leftMargin: set_languageIcon.width
                                rightMargin: set_languageIcon.width *3/8
                            }
                            y: set_aboutIcon.y+set_aboutIcon.height
                            color: "transparent"
                            Image {
                                anchors.fill: parent
                                source: "qrc:/img/IP_line.png"
                                fillMode: Image.Stretch
                            }
                        }
                        MouseArea {
                            width: set_main.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_helpIcon.bottom
                                left: parent.left
                            }
                            onClicked: {
                                wifiInfoProvider.updateWifiInfo()
                                stackLayout_more.currentIndex = moreDialog.aboutPageIndex
                                set_title_text.text=currentLanguage === "zh" ? qsTr("关于"): qsTr("About")
                                //btn_download.enabled=true
                                //progressText.text =""
                                //downloadStatusText.text=""
                                //progressText.visible=false
                                //downloadStatusText.visible=false
                            }
                        }

                        //用户协议图标
                        Rectangle {
                            id:set_uaIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_aboutIcon.bottom
                                left: parent.left
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_uaIcon.png";
                                fillMode: Image.PreserveAspectFit
                                width: set_uaIcon.width *5/8
                                height: set_uaIcon.height *50/103//set_languageIcon.height *23/50//anchors.centerIn:ipIcon// parent
                                anchors.centerIn: set_uaIcon
                            }
                        }
                        //用户协议文字
                        Text {
                            text:currentLanguage === "zh" ? qsTr("用户协议"): qsTr("User Agreement")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            color: "white"
                            font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 //anchors.centerIn: parent
                            x:set_uaIcon.width
                            anchors.verticalCenter: set_uaIcon.verticalCenter //垂直居中
                        }
                        //箭头图标
                        Rectangle {
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                right: parent.right
                                rightMargin: set_languageIcon.width *3/8
                                top:set_aboutIcon.bottom
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_right.png"  //自定义箭头图标
                                height: parent.height*0.2
                                anchors.right:parent.right
                                anchors.verticalCenter: parent.verticalCenter //垂直居中
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        //分割线04
                        Rectangle {
                            id:setLine04         //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                            width: parent.width-2-set_languageIcon.width *3/8
                            height: 1
                            anchors{
                                left:parent.left
                                right: parent.right
                                leftMargin: set_uaIcon.width
                                rightMargin: set_languageIcon.width *3/8
                            }
                            y: set_uaIcon.y+set_uaIcon.height
                            color: "transparent"
                            Image {
                                anchors.fill: parent
                                source: "qrc:/img/IP_line.png"
                                fillMode: Image.Stretch
                            }
                        }
                        MouseArea {
                            width: set_main.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_aboutIcon.bottom
                                left: parent.left
                            }
                            onClicked: {
                                if(currentLanguage === "zh" ){
                                    serverFinder.loadFile("qrc:/img/zh_yhxy.html")//loadFile("zh_yhxy.txt");// loadFile("zh_yhxy.html")
                                } else{
                                    serverFinder.loadFile("qrc:/img/en_yhxy.html");//permissionRequester.loadFile("qrc:/img/en_yhxy.txt");
                                }
                                // mainPage.visible=false;
                                // agreeRectangle.visible=true;
                                // currentPage="agreement"
                                stackLayout_more.currentIndex = moreDialog.agreementPageIndex
                                set_title_text.text=currentLanguage === "zh" ? qsTr("QUARCS应用程序用户协议"): qsTr("QUARCS Application User Agreement")
                            }
                        }

                        //隐私政策图标
                        Rectangle {
                            id:set_ppIcon            //anchors.horizontalCenter: parent.horizontalCenter//水平居中//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_uaIcon.bottom
                                left: parent.left
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_ppIcon.png";
                                fillMode: Image.PreserveAspectFit
                                width: set_ppIcon.width *5/8
                                height: set_ppIcon.height *50/103
                                anchors.centerIn: set_ppIcon
                            }
                        }
                        //隐私政策文字
                        Text {
                            text:currentLanguage === "zh" ? qsTr("隐私政策"): qsTr("Privacy Policy")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            color: "white"
                            font.pixelSize: aspectRatio>1.7 ?16:24//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 //anchors.centerIn: parent
                            x:set_ppIcon.width
                            anchors.verticalCenter: set_ppIcon.verticalCenter //垂直居中
                        }
                        //箭头图标
                        Rectangle {
                            width: set_languageIcon.width
                            height: set_languageIcon.height
                            anchors{
                                right: parent.right
                                rightMargin: set_languageIcon.width *3/8
                                top:set_uaIcon.bottom
                            }
                            color: "transparent"
                            Image {
                                source: "qrc:/img/set_right.png"  //自定义箭头图标
                                height: parent.height*0.2
                                anchors.right:parent.right
                                anchors.verticalCenter: parent.verticalCenter //垂直居中
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        MouseArea {//anchors.fill: parent
                            width: set_main.width
                            height: set_languageIcon.height
                            anchors{
                                top:set_uaIcon.bottom
                                left: parent.left
                            }
                            onClicked: {
                                if(currentLanguage === "zh" ){
                                    serverFinder.loadFile("qrc:/img/zh_yszc.html");  //loadFile("zh_yszc.txt");
                                } else{
                                    serverFinder.loadFile("qrc:/img/en_yszc.html");  //loadFile("en_yszc.txt");
                                }
                                // mainPage.visible=false;
                                // policyRectangle.visible=true;
                                // currentPage="policy"
                                stackLayout_more.currentIndex = moreDialog.policyPageIndex
                                set_title_text.text=currentLanguage === "zh" ? qsTr("隐私政策"): qsTr("Privacy Policy")
                            }
                        }
                    }
                    //版权信息区域
                    Rectangle {
                        width: set_connect.width
                        height: set_connect.height*7 / 62
                        //x: set_connect.width*30/ 1274
                        y: set_main.height+set_main.y
                        color: "transparent"
                        //版权信息
                        Text {
                            anchors.centerIn: parent
                            text:currentLanguage === "zh" ? qsTr("Copyright© 2025 光速视觉(北京)科技有限公司(QHYCCD)版权所有")
                                                          : qsTr("Copyright© 2025 Light Speed Vision (Beijing) Co., Ltd.(QHYCCD). All Rights Reserved.")
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            color: "white"
                            font.pixelSize: aspectRatio>1.7 ?12:18  //12//aspectRatio>1.7 ? Screen.width * 0.025 : Screen.width * 0.02 // // 宽屏设备、平板或方屏设备20
                        }
                    }
                }
                //帮助页面1
                Rectangle{
                    id:helpRectangle
                    width: parent.width*0.9
                    height: parent.height
                    anchors.centerIn: parent
                    color: "transparent"
                    // 使用 ScrollView 使文本内容可以滚动
                    ScrollView {
                        anchors.fill: parent//width: parent.width
                        clip: true  // 如果文本超出区域，则不显示
                            Text {
                                text:currentLanguage === "zh" ?
                                         qsTr("1、确保设备通电开机.\n 明确您的使用方式.\n2、盒子热点模式：需确保手机连接设备发出的热点WiFi信号.\n3、局域网模式：确保手机和设备连接同域的WLAN")
                                       :qsTr("1、Ensure that your devices are power on. \n Select the connection mode. \n2、StarMaster Pro Wi-Fi Mode: Ensure your phone is connected to the StarMaster Pro's Wi-Fi hotspot.\n3、WLAN Mode: Ensure both your phone and StarMaster Pro are connected to the same WLAN.")
                                font.pixelSize: aspectRatio>1.7 ?18:24//20
                                color: "white"
                                wrapMode: Text.Wrap
                                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                anchors.left: parent.left
                                anchors.right: parent.right
                            }
                    }
                }
                //关于页面2
                Rectangle{
                    id:aboutRectangle
                    width: parent.width*0.9
                    height: parent.height
                    anchors.centerIn: parent
                    color: "transparent"
                    // 使用 ScrollView 使文本内容可以滚动
                    ScrollView {
                        anchors {
                            top: parent.top
                            bottom: downloadStatus.top  // 关键：留出底部空间给进度条
                        }
                        width: parent.width
                        contentWidth: parent.width
                        contentHeight: innerColumn.implicitHeight
                        clip: true  // 如果文本超出区域，则不显示
                        // 设置一个 Column 用来管理文本内容
                        Column {//ColumnLayout Column
                            id:innerColumn
                            width: parent.width//anchors.fill:parent
                            spacing: 10
                            Text {
                                text: currentLanguage === "zh" ? "应用名称: "+ appName:"App Name: " + appName
                                font.pixelSize: aspectRatio>1.7 ?16:24//18
                                color: "white"
                                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter  // 设置标题的文本水平居中
                            }
                            Text {
                                text:currentLanguage === "zh" ? "版本: "+appVersion : "Version: "+appVersion
                                font.pixelSize: aspectRatio>1.7 ?16:24
                                color: "white"
                                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                width: parent.width// Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter  // 设置标题的文本水平居中
                            }
                            Text {
                                textFormat: Text.RichText
                                text:currentLanguage === "zh"
                                     ? "备案号: <a href='https://beian.miit.gov.cn'><font color='#006EFF'> 京ICP备18042925号-2A</a>"
                                     : "ICP Registration No.: <a href='https://beian.miit.gov.cn'><font color='#006EFF'>京ICP备18042925号-2A</a> "
                                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                                font.pixelSize:aspectRatio>1.7 ?16:24
                                color: "white"
                                // color: "#006EFF"
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter  // 设置标题的文本水平居中
                                onLinkActivated: {
                                    Qt.openUrlExternally(link)
                                }
                            }
                            Text {
                                text:currentLanguage === "zh" ?
                                         qsTr("    该APP版本\n     -支持QUARCS版本检测，APP增加小版本号的更新包下载与传输。\n     ")
                                       :qsTr("    This app version\n     - Supports QUARCS version detection and adds minor-version update package download and transfer within the app.\n")
                                font.pixelSize: aspectRatio>1.7 ?16:24
                                color: "white"
                                width: parent.width  // 必须设置宽度才能换行
                                wrapMode: Text.Wrap
                                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            }
                        }
                    }
                    Column {
                        id: downloadStatus
                        width: parent.width
                        // height: childrenRect.height  // 高度由子项决定，不强制约束
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10  // 距离底部 20px
                        spacing: 5
                        visible: true //false //true                        
                    }
                }
                
                //用户协议页面3//4
                Rectangle{
                    id:agreeRectangle
                    width: parent.width*0.95
                    height: parent.height
                    anchors.centerIn: parent
                    color: "transparent"
                    /**/
                    // 使用 ScrollView 使文本内容可以滚动
                    ScrollView {
                        width: parent.width
                        height: parent.height
                        contentWidth: parent.width
                        contentHeight: nameyh.implicitHeight
                        clip: true  // 如果文本超出区域，则不显示
                        Text {
                            id: nameyh
                            width: parent.width  // 文本宽度绑定到 ScrollView 的内容宽度
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            font.pixelSize: aspectRatio>1.7 ?16:24  // 添加字体大小
                            color: "white"
                            wrapMode: Text.Wrap  // 允许文本换行//width: parent.width*0.8  // 设置文本宽度，避免超出
                            text: serverFinder.fileContent
                        }
                    }
                }
                //隐私政策页面4//5
                Rectangle{
                    id:policyRectangle
                    width: parent.width*0.95
                    height: parent.height
                    anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    color: "transparent"
                    // 使用 ScrollView 使文本内容可以滚动
                    ScrollView {
                        width: parent.width
                        height: parent.height
                        contentWidth: parent.width// 限制内容宽度与 ScrollView 一致
                        contentHeight: nameys.implicitHeight
                        clip: true  // 如果文本超出区域，则不显示
                        Text {
                            id: nameys
                            width: parent.width  // 文本宽度绑定到 ScrollView 的内容宽度
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            font.pixelSize: aspectRatio>1.7 ?16:24  // 添加字体大小
                            color: "white"
                            wrapMode: Text.Wrap
                            text: serverFinder.fileContent
                        }
                    }
                }
            }
        }

        // 确保每次打开时重置到主页面
        onVisibleChanged: {
            if (visible) {
                stackLayout_more.currentIndex = moreDialog.mainPageIndex
                set_title_text.text = currentLanguage === "zh" ? qsTr("设置") : qsTr("Settings")
            }
        }
    }

    // 读取本地文件的辅助函数
    function readLocalFile(path) {
        var file = new XMLHttpRequest();
        file.open("GET", Qt.resolvedUrl("file://" + path), false); // 同步读取
        file.send();
        return file.status === 200 ? file.response : null;
    }

    function formatFileSize(bytes) {
        if (bytes < 1024) return bytes + " B";
        else if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        else if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB";
        else return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB";
    }

    //定位与时间信息页面
    Rectangle {
        id: locationDialog
        visible: false
        anchors.fill: parent
        color:  "transparent"
        // 当 visible 从 false 变成 true 时触发
        onVisibleChanged: {
            if (visible) {
                console.log("locationDialog定位页面显示")
                info_title_text.text=currentLanguage === "zh" ? qsTr("时间与定位"):qsTr("Time and Location")
                updateTime()
                positionSource.active = true
                positionSource.update()
                wifiInfoProvider.updateWifiInfo()
            }
        }
        //信息标题栏
        Rectangle {//1334*70  (0,0)
            id: info_title
            width:parent.width
            height:aspectRatio>1.7?parent.height*7/75:parent.height*5/75
            x:0
            y:0
            color:  "transparent"
            Image {
                id:_bg_title
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/Scan_title.png"
                opacity:1
            }
            //返回按钮
            Button{//17*30   (30,20)
                id:btn__back
                width: parent.width*47/1334
                height: parent.height
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                }
                background: Rectangle {
                    color: "transparent" // 背景色设置为透明
                }
                Image { //id: btn_back_icon
                    source: "qrc:/img/Btn_back.png"
                    width: title.width*0.013//30 // 图标的大小
                    height: title.height*3/7//30
                    anchors {
                        right: parent.right // 图标靠右
                        verticalCenter: parent.verticalCenter // 图标垂直居中//bottom: parent.bottom // 图标靠下
                        margins: 5 // 适当设置边距，使图标不紧贴按钮边缘
                    }
                }
                onClicked: {
                    //info_title_text.text=currentLanguage === "zh" ? qsTr("时间与定位"):qsTr("Time and Location")
                    locationDialog.visible=false;
                    startDialog.visible=true;
                }
            }
            //时间与定位 信息标题文字
            Text {  //anchors.verticalCenter: parent.verticalCenter //垂直居中
                id:info_title_text
                anchors.centerIn: parent//anchors.fill: parent     //x: parent.height*0.7
                text: currentLanguage === "zh" ? qsTr("时间与定位"):qsTr("Time and Location")
                font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                color: "white"
                font.pixelSize:aspectRatio>1.7 ?16:24
            }
        }
        //信息内容栏：经纬度、时间
        Rectangle {//1274*620  (30,100)
            id:info_connect
            width: parent.width*1274/1334
            height: aspectRatio>1.7?parent.height*62/75:parent.height*66/75
            x: parent.width*30/1334
            y:aspectRatio>1.7?parent.height*10/75:parent.height*7/75//parent.height*10/75
            color:  "transparent"
            // 设置背景图
            Image {
                source: "qrc:/img/Set_bg.png"
                anchors.fill: parent
                fillMode: Image.Stretch // 拉伸模式//opacity:1
            }
            //时间\经纬度显示
            Rectangle {//520*411  (center)
                id: timeConncet
                color:  "transparent"
                width: aspectRatio>1.7?parent.width*700/1274:parent.width*500/1274
                height:  aspectRatio>1.7?parent.height*500/620:parent.height*300/620
                anchors.centerIn: parent
                Image {
                    anchors.fill: parent // 图像填充整个窗口
                    source: "qrc:/img/IP_ImgBg.png"
                    fillMode: Image.Stretch
                    opacity:1
                }
                //时间显示文本
                Rectangle {//500*100 (700*500)
                    width: parent.width*5/7
                    height:parent.height*1/5//4/25
                    x:parent.width*100/700
                    y:parent.height*25/500
                    anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    color:"transparent"
                    Image {
                        anchors.fill: parent // 图像填充整个窗口
                        source: "qrc:/img/IP_input.png"
                        fillMode: Image.Stretch
                        opacity:1
                    }
                    Text {
                        anchors.centerIn: parent
                        font.pixelSize:aspectRatio>1.7 ?18:22
                        text:currentTime
                        color: "grey"
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto" //visible: locationInput.text.trim() === "" // 当输入框为空时显示
                    }
                }
                //纬度显示文本
                Rectangle {//500*100 (700*500)
                    width: parent.width*5/7
                    height:parent.height**1/5//4/25
                    x:parent.width*100/700
                    y:parent.height*150/500//130/500
                    anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    color:"transparent"
                    Image {
                        anchors.fill: parent // 图像填充整个窗口
                        source: "qrc:/img/IP_input.png"
                        fillMode: Image.Stretch
                        opacity:1
                    }
                    Text {
                        id:locationLat
                        anchors.centerIn: parent
                        font.pixelSize:aspectRatio>1.7 ?18:22
                        text:currentLat === 0?(currentLanguage === "zh" ? qsTr("纬度:未获取") : qsTr("Latitude: Not acquired")):(currentLanguage === "zh" ? qsTr("纬度:"+currentLat): qsTr("Latitude: "+currentLat))
                        color: "grey"
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto" //visible: locationInput.text.trim() === "" // 当输入框为空时显示
                    }
                }
                //经度显示文本
                Rectangle {//500*100 (700*500)
                    width: parent.width*5/7
                    height:parent.height*1/5//4/25
                    x:parent.width*100/700
                    y:parent.height*250/500//210/500
                    anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    color:"transparent"
                    Image {
                        anchors.fill: parent // 图像填充整个窗口
                        source: "qrc:/img/IP_input.png"
                        fillMode: Image.Stretch
                        opacity:1
                    }
                    Text {
                        id:locationLong
                        anchors.centerIn: parent
                        font.pixelSize:aspectRatio>1.7 ?18:22
                        text:currentLong === 0?(currentLanguage === "zh" ? qsTr("经度：未获取") : qsTr("Longitude: Not acquired")):(currentLanguage === "zh" ? qsTr("经度:"+currentLong): qsTr("Longitude: "+currentLong))
                        color: "grey"
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto" //visible: locationInput.text.trim() === "" // 当输入框为空时显示
                    }
                }
               /* //海拔显示文本
                Rectangle {//500*100 (700*500)
                    width: parent.width*5/7
                    height:parent.height*4/25
                    x:parent.width*100/700
                    y:parent.height*290/500
                    anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    color:"transparent"
                    Image {
                        anchors.fill: parent // 图像填充整个窗口
                        source: "qrc:/img/IP_input.png"
                        fillMode: Image.Stretch
                        opacity:1
                    }
                    Text {
                        id:locationAlt
                        anchors.centerIn: parent
                        font.pixelSize:aspectRatio>1.7 ?18:22
                        text:currentAlt === 0?(currentLanguage === "zh" ? qsTr("海拔:未获取") : qsTr("Alt.:Not acquired")):(currentLanguage === "zh" ? qsTr("海拔:"+currentAlt): qsTr("Alt.:"+currentAlt))
                        color: "grey"
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    }
                }*/

                //显示WiFi名称
                Rectangle {//500*100 (700*500)
                    width: parent.width*5/7
                    height:parent.height*1/5//4/25
                    x:parent.width*100/700
                    y:parent.height*375/500//395/500
                    anchors.horizontalCenter: parent.horizontalCenter//水平居中
                    color:"transparent"
                    Image {
                        anchors.fill: parent // 图像填充整个窗口
                        source: "qrc:/img/IP_input.png"
                        fillMode: Image.Stretch
                        opacity:1
                    }
                    Text {
                        id:wifitext
                        anchors.centerIn: parent
                        text:""
                        font.pixelSize: aspectRatio>1.7 ?18:22
                        color: "grey"
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    }
                }
                /*
                // //刷新按钮
                // Button {
                //     id: refreshButton
                //     visible: false
                //     width: parent.width*2/7
                //     height:parent.height*80/500
                //     y:parent.height*375/500
                //     anchors {
                //         left: parent.left
                //         leftMargin:parent.width*100/700
                //     }
                //     Image {
                //         id:btn_refresh
                //         anchors.fill: parent
                //         source: "transparent"
                //     }
                //     background: Image {
                //         id:btn_Img_refresh
                //         source: "qrc:/img/IP_Btn_enable.png";
                //         fillMode: Image.Stretch//fillMode: Image.PreserveAspectFit
                //         anchors.fill: parent
                //     }
                //     Text{
                //         text: currentLanguage === "zh" ? qsTr("刷新"): qsTr("Refresh")
                //         color: "white"
                //         font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                //         font.pixelSize: aspectRatio>1.7 ?16:20//Screen.width * 0.025
                //         anchors.centerIn: parent
                //     }
                //     onClicked: {
                //         updateTime()               //permissionRequester.requestPermissions() // 重新请求权限
                //         positionSource.update()
                //     }
                // }*/
                /*
                // //经纬度时间等信息上传服务端
                // Button {
                //     id: uploadButton
                //     visible: false
                //     width: parent.width*2/7
                //     height:parent.height*8/50
                //     y:parent.height*375/500
                //     anchors {
                //         right: parent.right
                //         rightMargin:parent.width*100/700
                //     }
                //     Image {
                //         id:btn_upload
                //         anchors.fill: parent
                //         source: "transparent"
                //     }
                //     background: Image {
                //         id:btn_Img_upload
                //         source: "qrc:/img/IP_Btn_enable.png";
                //         fillMode: Image.Stretch//fillMode: Image.PreserveAspectFit
                //         anchors.fill: parent
                //     }
                //     Text{
                //         text: currentLanguage === "zh" ? qsTr("上传"): qsTr("Upload")
                //         color: "white"
                //         font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                //         font.pixelSize: aspectRatio>1.7 ?16:20//Screen.width * 0.025
                //         anchors.centerIn: parent
                //     }
                //     onClicked: {
                //         //ToDo传递给服务端--经纬度、时间
                //     }
                // }
*/
            }
        }
   }


    function checkLocalUpdatePackages() {
        if (!hasFetchedVersions || !majorVersions || majorVersions.length === 0) {
            deviceHasLocalPackages = [];
            return;
        }
        if (!serverFinder || !ipList || ipList.length === 0) {
            deviceHasLocalPackages = [];
            deviceLocalPaths = [];
            return;
        }
        var matches = serverFinder.findLocalUpdatePackages(majorVersions) || [];
        var versionHitMap = {};
        matches.forEach(function(path) {
            if (!path)
                return;
            for (var j = 0; j < majorVersions.length; j++) {
                var version = majorVersions[j];
                if (!version)
                    continue;
                if (path.indexOf(version) !== -1) {
                    versionHitMap[version] = path;
                }
            }
        });
        var flags = [];
        var paths = [];
        for (var i = 0; i < ipList.length; i++) {
            var vh = (ipVersions && ipVersions.length > i && ipVersions[i]) ? ipVersions[i].trim() : "";
            if (!vh) {
                flags.push(false);
                paths.push("");
                continue;
            }
            var latestInfo = getLatestVersionForMajor(vh, majorVersions);
            var latest = latestInfo.version;
            var matchedPath = (latest && versionHitMap[latest]) ? versionHitMap[latest] : "";
            flags.push(!!matchedPath);
            paths.push(matchedPath);
        }
        deviceHasLocalPackages = flags;
        deviceLocalPaths = paths;
    }

    function markDeviceUploaded(ip) {
        if (!ip)
            return;
        var copy = ({})
        if (deviceUploadStatusMap) {
            for (var key in deviceUploadStatusMap) {
                if (deviceUploadStatusMap.hasOwnProperty(key)) {
                    copy[key] = deviceUploadStatusMap[key]
                }
            }
        }
        copy[ip] = true
        deviceUploadStatusMap = copy
    }

    function updateIPlistshow_color(){
        // 更新 listContent 的文本，改变选中 IP 的颜色
        var updatedText=""      //var linkText = currentLanguage === "zh" ? qsTr("虚拟设备"):qsTr("Virtual device")
        //var virColor = (selectedIp === "8.211.156.247") ? "white" : "#255db7"; // black如果选中虚拟设备
        if (ipList.length === 0) {
            scan_endText.text=currentLanguage === "zh" ? qsTr("未找到实体设备"):qsTr("No Physical Devices Detected")
        } else {
            // 改变虚拟设备的颜色          //updatedText +=qsTr("<a href='demo' style='color:" +virColor + "'>" +linkText +"</a><br>")
            scan_endText.text=currentLanguage === "zh" ? qsTr("已识别设备列表"):qsTr("Devices Detected")
            ipList.forEach(function(ip,index){// 改变设备列表IP 的颜色
                var color = (ip === selectedIp) ? "white" : "#006EFF"; // #255db7black选中 IP 的颜色
                //var versionValue = (ipVersions && ipVersions.length > index) ? ipVersions[index] : "";
                //var versionLabel = versionValue ? (" (Vh:" + versionValue + ")") : "";
                updatedText += "<a href='" + ip + "' style='color:" + color + "'>" + ip + "</a>";//+ versionLabel
                // 如果不是最后一个元素，添加 <br> 换行符
                if (index < ipList.length - 1) {updatedText += "<br>";}
            });
        }
        listContent.text = updatedText; // 更新列表内容
        //listUpdate.text =updatedText;
    }

    function validateIP(ip) {
        // 简单的IPv4地址校验
        var ipRegExp = /^(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)$/;
        //var ipRegExp = /^(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\:(0-9|[1-9][0-9]{1,4})$/;
        return ipRegExp.test(ip);
    }

    //网页
    Rectangle {
        id: webDialog
        visible: false
        width: parent.width
        height: parent.height
        anchors.fill: parent
        color:  "transparent"
        // 返回按钮（位于WebView的顶部）
        Button {
            id:btn_web_back
            width: parent.width*47/1334
            height: aspectRatio>1.7?parent.height*7/75:parent.height*5/75
            anchors {
                top: parent.top
                left: parent.left
            }
            //visible: false // 初始时隐藏返回按钮，直到网页加载完
            z: 2  // 确保按钮位于WebView之上
            background: Rectangle {
                color: "transparent" // 背景色设置为透明
            }
            Image {
                id: btn_Img_web_back
                source: "qrc:/img/Btn_back.png"
                width: parent.width*17/47
                height: parent.height*3/7
                anchors {
                    right: parent.right // 图标靠右
                    verticalCenter: parent.verticalCenter // 图标垂直居中//bottom: parent.bottom // 图标靠下
                    margins: 5 // 适当设置边距，使图标不紧贴按钮边缘
                }
            }
            onClicked: {
                webViewURL.stop();
                webViewURL.visible = false;  // 隐藏 WebView
                webDialog.visible = false;  // 隐藏对话框
                scanDialog.visible = !embeddedFrontendModeEnabled;
                scanDialog.enabled=true;
                startDialog.visible = embeddedFrontendModeEnabled;
                elapsedTime = 0;  // 重置计时
                timer.stop();  // 停止计时器
                btn_scanText.text = currentLanguage === "zh" ? qsTr("扫描"): qsTr("Scan");
                btn_scan.enabled = true;
                btn_Img_scan.opacity=1;
                btn_scan_back.enabled=true;
                btn_Img_scan_back.opacity=1;
                //isStopped = true;// 设置 isStopped 为 true，表示已经主动停止加载
                webFailRectangle.visible=false;
            }
        }
        Text{
            id:bgroundText
            font.family: (Qt.platform.os === "ios") ?"PingFang SC":"Roboto"//iOS默认中文字体
            font.pixelSize: aspectRatio>1.7 ?12:18//确保不设置时各平台字体大小一致
            x: parent.width / 22
            anchors.bottom: parent.bottom  // 底部对齐父容器的底部
            padding: 20 // 给文本添加一些内边距，使其不紧贴边界
            text: currentLanguage === "zh" ?qsTr("正在加载中···"):qsTr("Loading...") // 初始文本
            color: "white"
            visible: true // 初始时显示进度文本
        }
        //网页显示 WebView  WebEngineView不可用
        WebView{
            id:webViewURL
            width: parent.width
            height: parent.height
            anchors.fill: parent
            z: 1//anchors.centerIn: parent//anchors.fill: parent//
            visible: false // 初始化时先隐藏 WebView
            property bool hasErrorOccurred: false// 新增标志变量，用于记录是否已经处理过错误
            property bool isInitialized: false // 标志变量，用于记录是否已经执行过初始化函数
            property bool titleVerified: false // 标志变量，用于记录标题是否已验证正确
            property int titleCheckCount: 0 // 标题检查次数
            
            // 初始化函数：标题判断、设置window变量、注入qtObj，仅执行一次
            function initializeWebView() {
                // 如果标题已验证，不需要再检查
                if (titleVerified) {
                    return;
                }
                
                // 标题判断
                webViewURL.runJavaScript('
                    var title = document.title;
                    title;  // 返回标题供外部判断  //百度 QUARCS QHYCCD  // return "APP_msg"
                ', function(result) {
                    // 如果标题还未获取，进行二次判断
                    if (!result || result === "" || result === "undefined") {
                        titleCheckCount++;
                        console.log("标题还未获取，进行二次判断，检查次数:", titleCheckCount);
                        if (titleCheckCount < 3 && webViewURL.loadProgress >= 74) {
                            // 延迟后再次检查标题
                            Qt.callLater(function() {
                                webViewURL.initializeWebView();
                            });
                        } /*else if (titleCheckCount >= 3) {
                            // 多次检查后仍未获取标题，标记为错误
                            if (!webViewURL.hasErrorOccurred) {
                                webViewURL.hasErrorOccurred = true;
                                bgroundText.text=currentLanguage === "zh" ? qsTr("加载失败"): qsTr("Load failure")
                                console.log("多次检查后仍未获取标题，加载失败")
                                var errorMsgip = currentLanguage === "zh" ?
                                            qsTr("无法获取网页标题，请确认"):
                                            qsTr("Unable to get webpage title, please confirm.")
                                webFailRectangle.errorDetail = errorMsgip;
                                webFailRectangle.visible = true;
                            }
                        }
                        return;*/
                    }
                    
                    if (result && result.includes("QUARCS")) {
                        // 只有在未初始化时才设置变量和注入qtObj
                        if (!isInitialized) {
                            var injectedStateScript =
                                    "window.currentTime = " + JSON.stringify(String(currentTime)) + ";" +
                                    "window.currentLat = " + JSON.stringify(String(currentLat)) + ";" +
                                    "window.currentLong = " + JSON.stringify(String(currentLong)) + ";" +
                                    "window.currentLanguage = " + JSON.stringify(String(currentLanguage)) + ";" +
                                    "window.wifiname = " + JSON.stringify(String(wifiInfoProvider.wifiInfo)) + ";" +
                                    "window.appVersion = " + JSON.stringify(String(appVersion)) + ";";
                            // 设置window变量
                            webViewURL.runJavaScript(injectedStateScript);
                            // 注入qtObj
                            webViewURL.runJavaScript('
                                window.qtObj = {
                                    showMessage: function(msg) {
                                        alert("收到Vue消息:"+ msg);
                                    },
                                    getMyValue: function() {
                                        return JSON.stringify({
                                            type: "APP_msg",
                                            time:window.currentTime,
                                            lat:window.currentLat,
                                            lon:window.currentLong,
                                            language:window.currentLanguage,
                                            wifiname:window.wifiname,
                                            appversion:window.appVersion,
                                        });
                                    }
                                }
                                window.qtObjInjected = true;
                                console.log("Injected qtObj:", window.qtObj);
                            ');
                            isInitialized = true; // 标记为已初始化
                        }
                        titleVerified = true; // 标记标题已验证
                        
                        // 当标题正确且加载超过74时显示网页
                        // 使用 Qt.callLater 确保在下一个事件循环中检查进度
                        Qt.callLater(function() {
                            if (webViewURL.loadProgress >= 74 && webViewURL.titleVerified) {
                                elapsedTime = 0;  // 重置计时
                                timer.stop();  // 停止计时器
                                bgroundText.text=currentLanguage === "zh" ? qsTr("加载成功"): qsTr("Load success")
                                console.log("标题验证成功，显示网页，进度:",webViewURL.loadProgress)
                                webViewURL.visible = true
                                webViewURL.height = parent.height
                                webViewURL.y = 0
                                serverFinder.findClose();
                            }
                        });
                    }
                    else {
                        if (!webViewURL.hasErrorOccurred) {
                            webViewURL.hasErrorOccurred = true; // 标记已处理错误
                            bgroundText.text=currentLanguage === "zh" ? qsTr("加载失败"): qsTr("Load failure")
                            console.log("onLoadProgressChanged :",webViewURL.loadProgress,"failure")
                            var errorMsgip = currentLanguage === "zh" ?
                                        qsTr("非QUARCS网页，请确认"):
                                        qsTr("This is not a QUARCS webpage, please confirm.")
                            webFailRectangle.errorDetail = errorMsgip; // 更新错误详情
                            webFailRectangle.visible = true;
                            // webSocket.active = false;
                        }
                    }
                });
            }
            
            // 监听 loadProgress 信号，更新进度
            onLoadProgressChanged: {
                console.log("onLoadProgressChanged :", webViewURL.url,"xx%:",webViewURL.loadProgress,"hasErrorOccurred:",webViewURL.hasErrorOccurred)//,"Loading changed - Status:",loadRequest.status
                if(!timer.running)
                {
                    bgroundText.text=currentLanguage === "zh" ? qsTr("页面刷新中···" +" ("+webViewURL.loadProgress+"%)"): qsTr("Refreshing..." +" ("+webViewURL.loadProgress+"%)")
                }
                if(webViewURL.loadProgress<74){
                    webViewURL.visible = false;
                    console.log("onLoadProgressChanged :",webViewURL.loadProgress)
                }else{
                    console.log("onLoadProgressChanged   hasErrorOccurred:",webViewURL.hasErrorOccurred)
//                    bgText.text+=qsTr(webViewURL.loadProgress+">74 ")//+isStopped                    
                    if (!webViewURL.hasErrorOccurred) { // 仅当未处理过错误时才检查
                        // 调用初始化函数（仅执行一次）
                        webViewURL.initializeWebView();
                        
                        // 如果标题已验证且加载超过74，显示网页
                        if (webViewURL.loadProgress >= 74 && webViewURL.titleVerified && !webViewURL.visible){
                            elapsedTime = 0;  // 重置计时
                            timer.stop();  // 停止计时器
                            bgroundText.text=currentLanguage === "zh" ? qsTr("加载成功"): qsTr("Load success")
                            console.log("onLoadProgressChanged :",webViewURL.loadProgress,"success, title verified, show webview")
                            webViewURL.visible = true
                            webViewURL.height = parent.height
                            webViewURL.y = 0
                            serverFinder.findClose();
                        }
                    }else{
                        elapsedTime = 0;  // 重置计时
                        timer.stop();  // 停止计时器
                    }
                }
            }

            onLoadingChanged:function(loadRequest){
                console.log(
                            "Loading changed - Status:", loadRequest.status,
                            "Error:", loadRequest.error,
                            "URL:", loadRequest.url
                            )
                // 当页面开始加载时，重置标志变量
                if (loadRequest.status === 0) { // 00 表示开始加载
                    isInitialized = false;
                    titleVerified = false;
                    titleCheckCount = 0;
                    hasErrorOccurred = false;
                    console.log("页面开始加载，重置标志变量");
                }
                var errorMsg="";
                if (loadRequest.status===3) {
                    console.error("onLoadingChanged status=3", loadRequest.errorString)
                    hasErrorOccurred = true;
                    // console.log("1 - Before assignment"); // 调试点1
                    errorMsg = currentLanguage === "zh" ?
                                qsTr("连接超时 (错误代码: %1)").arg(loadRequest.errorString) :
                                qsTr("Connection timed out (Error code: %1)").arg(loadRequest.errorString);
                    webFailRectangle.errorDetail = errorMsg; // 更新错误详情
                    // console.log("2 - After assignment  Error detail set:", errorMsg); // 调试输出
                    webFailRectangle.visible = true;
                }
                else if (loadRequest.error) {
                    console.log("加载失败:", loadRequest.errorString);
                    hasErrorOccurred = true;
                    errorMsg = currentLanguage === "zh" ?
                                qsTr("加载失败 (错误代码: %1)\n 请重试").arg(loadRequest.errorString) :
                                qsTr("Load Failed(Error code: %1)\n Please try again").arg(loadRequest.errorString);
                    webFailRectangle.errorDetail = errorMsg; // 更新错误详情
                    webFailRectangle.visible = true;
                }
            }

        }
        Connections {
            target: serverFinder
            function onServerClose() {
                console.log("onServerClose html->quit")
                timer.stop();  // 停止计时器
                elapsedTime = 0;  // 重置计时
                scanDialog.visible=true;
                webDialog.visible=false;
            }
        }
        Connections {
            target: compatibilityCommandServer
            function onCloseWebViewRequested() {
                webViewURL.stop();
                webViewURL.visible = false;
                webDialog.visible = false;
                webFailRectangle.visible = false;
                if (embeddedFrontendModeEnabled) {
                    startDialog.visible = true;
                } else {
                    scanDialog.visible = true;
                }
            }
        }
        onVisibleChanged: {
            if (visible) {
                bgroundText.text=currentLanguage === "zh" ?qsTr("正在加载中···"):qsTr("Loading...")
                elapsedTime = 0;  // 重置计时
                timer.start();  // 开始计时器
                scanDialog.visible=false;
            }
        }
    }

    //加载页面失败提示
    Rectangle {
        id: webFailRectangle
        property string errorDetail: ""
        visible: false
        anchors.fill: parent
        color:  "transparent"
        Image {
            id:bg_webFail
            anchors.fill: parent
            source: "qrc:/img/IP_bg.png"//scan_IPbg   Scan_result
            fillMode: Image.Stretch // 拉伸模式//  fillMode: Image.PreserveAspectFit//
            opacity:1
        }
        Rectangle {
            id: webFailConncet
            color:  "transparent"
            width: parent.width*520/1334
            height: parent.height*411/750
            anchors.centerIn: parent
            Image {            //id:bg_IP
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/IP_ImgBg.png"
                fillMode: Image.Stretch
                opacity:1
            }
            //标题
            Rectangle {
                id:failTitle            //anchors.horizontalCenter: parent.horizontalCenter//水平居中//anchors.verticalCenter: wifiModeIcon.verticalCenter //垂直居中
                width: parent.width
                height: parent.height*80/411
                color:  "transparent"
                Text {
                    text:currentLanguage === "zh" ? qsTr("错误提示"): qsTr("Error Message")//连接失败Connection Failed
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    color: "white"
                    font.pixelSize:aspectRatio>1.7 ?16:24
                    anchors.centerIn: parent
                }
            }
            //分割线
            Rectangle {
                id:webFailLine            //anchors.horizontalCenter: parent.horizontalCenter//水平居中
                width: parent.width-2
                height: 1
                anchors{
                    left:parent.left
                    right: parent.right
                    leftMargin: 1
                    rightMargin: 1
                }
                y: parent.height*80/411
                color: "transparent"
                Image {
                    anchors.fill: parent
                    source: "qrc:/img/IP_line.png"
                    fillMode: Image.Stretch
                }
            }
            //提示内容
            Rectangle {
                width: parent.width*500/520
                height:parent.height*230/411
                x:parent.width*10/52
                y:parent.height*90/411
                anchors.horizontalCenter: parent.horizontalCenter//水平居中
                color:"transparent"
                ScrollView {
                    clip: true  // 如果文本超出区域，则不显示
                    anchors.fill: parent
                        Text {
                            id: errorContentText
                            wrapMode: Text.Wrap  // 允许文本换行
                            text:{return (webFailRectangle.errorDetail ? webFailRectangle.errorDetail : "");}
                            font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                            font.pixelSize: aspectRatio>1.7 ?16:24
                            anchors.left: parent.left
                            anchors.right: parent.right//horizontalAlignment: Text.AlignHCenter  // 设置标题的文本水平居中
                            color: "white"
                        }
                }
            }
            //按钮
            Rectangle {
                id: closeButton
                width:parent.width-2
                height:parent.height*77/411
                y:parent.height*332/411
                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                radius: 5  // 圆角半径
                color: "#006EFF"
                Button {
                    anchors.fill: parent
                    background: Rectangle {
                        color: "transparent"
                    }
                    Text{
                        text: currentLanguage === "zh" ? qsTr("关闭"): qsTr("Exit")//已知晓Dismiss
                        color: "white"
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                        font.pixelSize: aspectRatio>1.7 ?16:24
                        anchors.centerIn: parent
                    }
                    onClicked: {
                        webFailRectangle.visible = false
                        webDialog.enabled=true
                        webDialog.visible=false
                        scanDialog.visible=true
                        scanDialog.enabled=true

                        btn_scan.clicked()//避免更换wifi后列表不更新问题
                    }
                }
            }
        }

        onVisibleChanged: {
            if(visible){
                elapsedTime = 0;  // 重置计时
                timer.stop();
                webViewURL.visible = false;
                webDialog.enabled=false;
                scanDialog.enabled=false;
            }
        }
    }

    //仅提示页面
    Rectangle {
        id: tipRectangle
        property string errorDetail: ""
        visible: false
        anchors.fill: parent
        color:  "transparent"
        Image {
            id:bg_tip
            anchors.fill: parent
            source: "qrc:/img/IP_bg.png"//scan_IPbg   Scan_result
            fillMode: Image.Stretch // 拉伸模式//  fillMode: Image.PreserveAspectFit//
            opacity:1
        }
        Rectangle {
            id: tipConncet
            color:  "transparent"
            width: parent.width*520/1334
            height: parent.height*411/750
            anchors.centerIn: parent
            Image {            //id:bg_IP
                anchors.fill: parent // 图像填充整个窗口
                source: "qrc:/img/IP_ImgBg.png"
                fillMode: Image.Stretch
                opacity:1
            }
            //标题
            Rectangle {
                id:tipTitle
                width: parent.width
                height: parent.height*80/411
                color:  "transparent"
                Text {
                    text:currentLanguage === "zh" ? qsTr("提示"): qsTr("Tip")
                    font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                    color: "white"
                    font.pixelSize:aspectRatio>1.7 ?16:24
                    anchors.centerIn: parent
                }
            }
            //分割线
            Rectangle {
                id:tipLine 
                width: parent.width-2
                height: 1
                anchors{
                    left:parent.left
                    right: parent.right
                    leftMargin: 1
                    rightMargin: 1
                }
                y: parent.height*80/411
                color: "transparent"
                Image {
                    anchors.fill: parent
                    source: "qrc:/img/IP_line.png"
                    fillMode: Image.Stretch
                }
            }
            //提示内容
            Rectangle {
                width: parent.width*500/520
                height:parent.height*230/411
                x:parent.width*10/52
                y:parent.height*90/411
                anchors.horizontalCenter: parent.horizontalCenter//水平居中
                color:"transparent"
                ScrollView {
                    clip: true  // 如果文本超出区域，则不显示
                    anchors.fill: parent
                    Text {
                        id: tipContentText
                        wrapMode: Text.Wrap  // 允许文本换行
                        text:{return (tipRectangle.errorDetail ? tipRectangle.errorDetail : "");}

                            //tipRectangle.errorDetail
                            /*{// 优先显示错误详情（如果有）
                                var baseText = currentLanguage === "zh" ?
                                            qsTr("1、主动中断连接时可重新点击进行连接 \n2、因IP无效加载失败时请输入正确的设备 IP\n3、盒子热点方式:重新连接设备发出的热点WiFi\n4、局域网方式:重新连接设备所连接的同局域网的WLAN\n5、其他情况可重新打开软件或重启手机") :
                                            qsTr("1、The connection is actively interrupted.Click to reconnect.\n2、Loading failed due to an invalid IP.  Please enter the correct StarMaster Pro IP.\n
3、StarMaster Pro Wi-Fi Mode: Reconnect your phone to the StarMaster Pro's Wi-Fi hotspot.\n4、WLAN Mode: Reconnect your phone and StarMaster Pro to the same WLAN. \n
5、If other issues occur, you can reopen the software or restart the phone. ");
                                return (webFailRectangle.errorDetail ? webFailRectangle.errorDetail + "\n\n" : "") + baseText;
                            }*/
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                        font.pixelSize: aspectRatio>1.7 ?16:24
                        anchors.left: parent.left
                        anchors.right: parent.right
                        color: "white"
                    }
                }
            }
            //按钮
            Rectangle {
                id: knowButton
                width:parent.width-2
                height:parent.height*77/411
                y:parent.height*332/411
                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                radius: 5  // 圆角半径
                color: "#006EFF"
                Button {
                    anchors.fill: parent
                    background: Rectangle {
                        color: "transparent"
                    }
                    Text{
                        text: currentLanguage === "zh" ? qsTr("已知晓"): qsTr("Got it")//已知晓Dismiss
                        color: "white"
                        font.family: (Qt.platform.os === "ios") ? "PingFang SC" : "Roboto"
                        font.pixelSize:aspectRatio>1.7 ?16:24
                        anchors.centerIn: parent
                    }
                    onClicked: {
                        //更新显示打开选中IP网页
                        tipAutoHideTimer.stop()
                        scanDialog.enabled = true
                        tipRectangle.visible = false
                        updateIPlistshow_color();
                        webViewURL.visible=true
                        // webSocket.url = "ws://" + selectedIp + ":8600/";
                        // webSocket.active = true;
                    }
                }
            }
        }
    }
    Timer {
        id: tipAutoHideTimer
        interval: 3000
        repeat: false
        onTriggered: {
            tipRectangle.visible = false
            scanDialog.enabled = true
        }
    }

    // 保存当前语言设置
    function saveLanguageSettings() {
        languageSettings.language = currentLanguage
    }

    // 更新时间函数
    function updateTime() {
        var date = new Date()
        //currentTime = date.toLocaleTimeString(Qt.locale(), "yyyy-MM-dd:hh:mm:ss")
        var curDate = date.toLocaleDateString(Qt.locale(), "yyyy-MM-dd")
        var curTime = date.toLocaleTimeString(Qt.locale(), "hh:mm:ss")
        currentTime = curDate + ":" + curTime  // 中间用空格分隔
    }

    //获取指定网页txt中存在的各大版本号对应的最新版本号(txt文件中每行写一个大版本号的最新版本号，不可有其他内容)
    function fetchMajorVersions() {
           if (!versionListUrl || versionListUrl === "" || hasFetchedVersions)
               return;
           var xhr = new XMLHttpRequest();
           xhr.onreadystatechange = function() {
               if (xhr.readyState === XMLHttpRequest.DONE) {
                   if (xhr.status === 200) {
                       var raw = xhr.responseText;
                       var lines = raw.split(/\r?\n/);
                       var majorMap = {};
                       lines.forEach(function(line) {
                           var v = line.trim();
                           if (!v)
                               return;
                           var parts = v.split(".");
                           if (!parts.length)
                               return;
                           var key = parts[0] + ".x";
                           if (!majorMap[key])
                               majorMap[key] = v;   // 只保存每个大版本的第一个值
                       });
                       majorVersions = Object.values(majorMap);
                       hasFetchedVersions = majorVersions.length > 0;
                       console.log("Fetched major versions:", majorVersions);
                   } else {
                       console.warn("Fetch version list fail:", xhr.status);
                   }
               }    
           };
           xhr.open("GET", versionListUrl);
           xhr.send();
       }

    //版本号比较函数
    function compareVersions(v1, v2) {
        if (!v1 || !v2)
            return 0;
        var a = v1.split(".");
        var b = v2.split(".");
        var len = Math.max(a.length, b.length);
        for (var i = 0; i < len; i++) {
            var n1 = parseInt(a[i], 10);
            var n2 = parseInt(b[i], 10);
            if (isNaN(n1)) n1 = 0;
            if (isNaN(n2)) n2 = 0;
            if (n1 !== n2)
                return n1 - n2;
        }
        return 0;
    }

    //筛选大版本号匹配的最新版本号
    function getLatestVersionForMajor(vh, versions) {
        var result = {
            version: "",
            hasHigherMajor: false//, higherMajorVersion: ""
        };
        if (!vh || !versions || versions.length === 0){
            return result;
        }

        var major = vh.split(".")[0];
        if (!major){
            return result;
        }

        var majorNumber = parseInt(major, 10);
        //var minHigherMajor = Number.MAX_SAFE_INTEGER;

        for (var i = 0; i < versions.length; i++) {
            var candidate = versions[i];
            if (!candidate)
                continue;

            var candidateMajorStr = candidate.split(".")[0];
            if (!candidateMajorStr)
                continue;

            if (candidateMajorStr === major) {
                result.version = candidate.trim();
                continue;//break;
            }

            var candidateMajorNum = parseInt(candidateMajorStr, 10);
            if (!isNaN(candidateMajorNum) && !isNaN(majorNumber) && candidateMajorNum > majorNumber) {
                result.hasHigherMajor = true;
            }
        }

        return result;
    }

    function showTemporaryTip(message) {
        if (!message)
            return;
        tipRectangle.errorDetail = message;
        tipRectangle.visible = true;
        scanDialog.enabled = false;
        tipAutoHideTimer.stop();
        tipAutoHideTimer.start();
    }

}



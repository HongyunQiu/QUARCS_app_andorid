TEMPLATE = app
#VERSION = 1.0.7
QT += webview
QT += network websockets
QT += positioning
QT += core quick  positioningquick
QT += bluetooth
# QMAKE_CXXFLAGS += -Dfile.encoding=UTF-8

CONFIG += c++17
DEFINES += QT_DEPRECATED_WARNINGS

android {
    QHY_SDK_DIR = /home/q/workspace/QHYCCD_SDK_CrossPlatform
    LIBUSB_ANDROID_DIR = $$QHY_SDK_DIR/libusb-1.0.android

    exists($$LIBUSB_ANDROID_DIR/android/config.h) {
        # libusb and QHY SDK both ship a config.h. Keep libusb's Android
        # config first so libusbi.h resolves the correct platform defines.
        INCLUDEPATH = $$LIBUSB_ANDROID_DIR/android $$INCLUDEPATH
        INCLUDEPATH += $$LIBUSB_ANDROID_DIR/libusb
        INCLUDEPATH += $$LIBUSB_ANDROID_DIR/libusb/os
    }
    exists($$QHY_SDK_DIR/src/qhyccd.h) {
        INCLUDEPATH += $$QHY_SDK_DIR/src
    }
    exists($$QHY_SDK_DIR/qhy_jni/aarch64/libqhyccd.a) {
        LIBS += $$QHY_SDK_DIR/qhy_jni/aarch64/libqhyccd.a
    }

    SOURCES += \
        $$LIBUSB_ANDROID_DIR/libusb/core.c \
        $$LIBUSB_ANDROID_DIR/libusb/descriptor.c \
        $$LIBUSB_ANDROID_DIR/libusb/hotplug.c \
        $$LIBUSB_ANDROID_DIR/libusb/io.c \
        $$LIBUSB_ANDROID_DIR/libusb/sync.c \
        $$LIBUSB_ANDROID_DIR/libusb/strerror.c \
        $$LIBUSB_ANDROID_DIR/libusb/os/linux_usbfs.c \
        $$LIBUSB_ANDROID_DIR/libusb/os/events_posix.c \
        $$LIBUSB_ANDROID_DIR/libusb/os/threads_posix.c \
        $$LIBUSB_ANDROID_DIR/libusb/os/linux_netlink.c
}

SOURCES += \
        apphostservice.cpp \
        compatibilitycommandserver.cpp \
        debuglogservice.cpp \
        serverfinder.cpp \
        bluetoothscanner.cpp \
        devicegatewayservice.cpp \
        embeddedassethttpserver.cpp \
        filterwheelservice.cpp \
        focuserservice.cpp \
        hardware/adapters/camera/default_camera_device.cpp \
        hardware/adapters/camera/qhyccd_camera_adapter.cpp \
        hardware/adapters/focuser/default_focuser_device.cpp \
        hardware/adapters/focuser/qfocuser_adapter.cpp \
        hardware/adapters/mount/default_mount_device.cpp \
        hardware/adapters/mount/onstep_mount_adapter.cpp \
        hardware/core/camera/qhyccd/qhyccd_camera_core.cpp \
        hardware/core/camera/qhyccd/qhyccd_camera_sdk_bridge.cpp \
        hardware/core/mount/onstep/onstep_mount_core.cpp \
        hardware/core/mount/onstep/onstep_mount_protocol.cpp \
        hardware/core/focuser/qfocuser/qfocuser_core.cpp \
        hardware/core/focuser/qfocuser/qfocuser_protocol.cpp \
        hardware/devices/device_registry.cpp \
        hardware/transports/tcp_transport.cpp \
        hardware/transports/usb_serial_transport.cpp \
        guiderservice.cpp \
        imagingservice.cpp \
        mountservice.cpp \
        platesolveservice.cpp \
        qtbluetoothscanner.cpp \
        scheduleservice.cpp \
        wifiinfoprovider.cpp

RESOURCES += \
        qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


HEADERS += \
    apphostservice.h \
    compatibilitycommandserver.h \
    debuglogservice.h \
    compatcommandservice.h \
    serverfinder.h \
    bluetoothscanner.h \
    devicegatewayservice.h \
    embeddedassethttpserver.h \
    filterwheelservice.h \
    focuserservice.h \
    hardware/adapters/camera/default_camera_device.h \
    hardware/adapters/camera/qhyccd_camera_adapter.h \
    hardware/adapters/focuser/default_focuser_device.h \
    hardware/adapters/focuser/qfocuser_adapter.h \
    hardware/adapters/mount/default_mount_device.h \
    hardware/adapters/mount/onstep_mount_adapter.h \
    hardware/core/camera/qhyccd/qhyccd_camera_core.h \
    hardware/core/camera/qhyccd/qhyccd_camera_sdk_bridge.h \
    hardware/core/focuser/qfocuser/qfocuser_core.h \
    hardware/core/focuser/qfocuser/qfocuser_protocol.h \
    hardware/core/mount/onstep/onstep_mount_core.h \
    hardware/core/mount/onstep/onstep_mount_protocol.h \
    hardware/devices/camera_device_interface.h \
    hardware/devices/device_registry.h \
    hardware/devices/focuser_device_interface.h \
    hardware/devices/mount_device_interface.h \
    hardware/transports/tcp_transport.h \
    hardware/transports/transport_interface.h \
    hardware/transports/usb_serial_transport.h \
    guiderservice.h \
    imagingservice.h \
    mountservice.h \
    platesolveservice.h \
    qtbluetoothscanner.h \
    scheduleservice.h \
    wifiinfoprovider.h

DISTFILES +=\
    android-sources/AndroidManifest.xml \
    android-sources/LOGOq.png\
    ios-sources/Asset.xcassets/AppIcon.appiconset/Contents.json \
    ios-sources/Asset.xcassets/AppIcon.appiconset/LOGOios.jpg \
    ios-sources/Asset.xcassets/Contents.json \
    ios-sources/Info.plist \
    ios-sources/LOGOios.jpg
android{
    SOURCES+=main.cpp \
        PermissionRequester.cpp
    HEADERS += \
        PermissionRequester.h
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android-sources
    win32:exists(C:/Qt6/Android/android_openssl-master/openssl.pri) {
        include(C:/Qt6/Android/android_openssl-master/openssl.pri)
    } else {
        message("[WARN] android_openssl openssl.pri not found; continue without explicit openssl.pri")
    }

}
ios{
    HEADERS += ioswifihelper.h
    LIBS += -framework SystemConfiguration
    LIBS += -framework CoreFoundation
    LIBS += -framework CoreLocation
    LIBS += -framework CoreBluetooth
    SOURCES+=main.mm \
        ioswifihelper.mm
    QMAKE_INFO_PLIST = ios-sources/Info.plist
}

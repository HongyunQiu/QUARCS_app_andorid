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
SOURCES += \
        apphostservice.cpp \
        compatibilitycommandserver.cpp \
        serverfinder.cpp \
        bluetoothscanner.cpp \
        devicegatewayservice.cpp \
        embeddedassethttpserver.cpp \
        filterwheelservice.cpp \
        focuserservice.cpp \
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
    compatcommandservice.h \
    serverfinder.h \
    bluetoothscanner.h \
    devicegatewayservice.h \
    embeddedassethttpserver.h \
    filterwheelservice.h \
    focuserservice.h \
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

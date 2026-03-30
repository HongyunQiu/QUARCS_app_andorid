#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

class ICameraDevice;
class IFocuserDevice;
class IMountDevice;

class DeviceRegistry
{
public:
    static ICameraDevice &cameraDevice();
    static IMountDevice &mountDevice();
    static IFocuserDevice &focuserDevice();
    static void reloadCameraDevice();
    static void reloadMountDevice();
    static void reloadFocuserDevice();
};

#endif // DEVICE_REGISTRY_H

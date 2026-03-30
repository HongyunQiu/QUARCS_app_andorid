#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

class IFocuserDevice;
class IMountDevice;

class DeviceRegistry
{
public:
    static IMountDevice &mountDevice();
    static IFocuserDevice &focuserDevice();
    static void reloadMountDevice();
    static void reloadFocuserDevice();
};

#endif // DEVICE_REGISTRY_H

package org.qtproject.example.QUARCS_app;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;

import org.qtproject.qt.android.QtNative;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class QhyUsbHelper {
    private static final String TAG = "QhyUsbHelper";
    private static final String ACTION_USB_PERMISSION = "org.qtproject.example.QUARCS_app.QHY_USB_PERMISSION";
    private static final int VENDOR_QHYCCD = 0x1618;
    private static final Object LOCK = new Object();

    private static boolean receiverRegistered = false;
    private static final Map<String, UsbDeviceConnection> openConnections = new HashMap<>();

    private QhyUsbHelper() {
    }

    public static String describeQhyDevices() {
        synchronized (LOCK) {
            UsbManager manager = getUsbManager();
            if (manager == null) {
                return "usbManager=null";
            }
            List<UsbDevice> devices = findQhyDevices(manager);
            if (devices.isEmpty()) {
                return "none";
            }
            return buildSummary(manager, devices);
        }
    }

    public static boolean prepareForCameraScan(int timeoutMs) {
        synchronized (LOCK) {
            UsbManager manager = getUsbManager();
            Activity activity = QtNative.activity();
            if (manager == null || activity == null) {
                Log.w(TAG, "prepareForCameraScan: missing activity or usbManager");
                return false;
            }

            ensureReceiverRegistered(activity);

            long deadline = SystemClock.elapsedRealtime() + Math.max(timeoutMs, 0);
            String lastStableSignature = "";
            long stableSince = 0L;

            while (true) {
                List<UsbDevice> devices = findQhyDevices(manager);
                boolean allGranted = !devices.isEmpty();
                boolean requestedAnyPermission = false;

                for (UsbDevice device : devices) {
                    if (!manager.hasPermission(device)) {
                        requestPermission(activity, manager, device);
                        requestedAnyPermission = true;
                        allGranted = false;
                    }
                }

                if (allGranted) {
                    ensureOpenConnections(manager, devices);
                    String signature = buildSignature(devices);
                    long now = SystemClock.elapsedRealtime();
                    if (!signature.equals(lastStableSignature)) {
                        lastStableSignature = signature;
                        stableSince = now;
                    }
                    if (now - stableSince >= 800L) {
                        Log.i(TAG, "QHY USB ready: " + buildSummary(manager, devices));
                        return true;
                    }
                } else {
                    lastStableSignature = "";
                    stableSince = 0L;
                }

                if (SystemClock.elapsedRealtime() >= deadline) {
                    if (requestedAnyPermission) {
                        Log.i(TAG, "QHY USB permission still pending: " + buildSummary(manager, devices));
                    } else {
                        Log.i(TAG, "QHY USB prepare timeout: " + buildSummary(manager, devices));
                    }
                    return hasReadyDevice(manager);
                }

                SystemClock.sleep(250L);
            }
        }
    }

    public static String getQhyDeviceRecords() {
        synchronized (LOCK) {
            UsbManager manager = getUsbManager();
            if (manager == null) {
                return "";
            }
            List<UsbDevice> devices = findQhyDevices(manager);
            ensureOpenConnections(manager, devices);
            StringBuilder builder = new StringBuilder();
            for (UsbDevice device : devices) {
                UsbDeviceConnection connection = openConnections.get(device.getDeviceName());
                if (connection == null) {
                    continue;
                }
                if (builder.length() > 0) {
                    builder.append('\n');
                }
                builder.append(device.getDeviceName())
                        .append('\t')
                        .append(device.getVendorId())
                        .append('\t')
                        .append(device.getProductId())
                        .append('\t')
                        .append(connection.getFileDescriptor());
            }
            return builder.toString();
        }
    }

    private static UsbManager getUsbManager() {
        Activity activity = QtNative.activity();
        if (activity == null) {
            return null;
        }
        return (UsbManager) activity.getSystemService(Context.USB_SERVICE);
    }

    private static boolean hasReadyDevice(UsbManager manager) {
        for (UsbDevice device : findQhyDevices(manager)) {
            if (manager.hasPermission(device)) {
                return true;
            }
        }
        return false;
    }

    private static void ensureOpenConnections(UsbManager manager, List<UsbDevice> devices) {
        List<String> activeNames = new ArrayList<>();
        for (UsbDevice device : devices) {
            activeNames.add(device.getDeviceName());
            if (!manager.hasPermission(device)) {
                continue;
            }
            UsbDeviceConnection existing = openConnections.get(device.getDeviceName());
            if (existing != null) {
                continue;
            }
            UsbDeviceConnection connection = manager.openDevice(device);
            if (connection != null) {
                openConnections.put(device.getDeviceName(), connection);
            }
        }

        List<String> staleNames = new ArrayList<>(openConnections.keySet());
        for (String name : staleNames) {
            if (activeNames.contains(name)) {
                continue;
            }
            UsbDeviceConnection stale = openConnections.remove(name);
            if (stale != null) {
                try {
                    stale.close();
                } catch (Exception ignored) {
                }
            }
        }
    }

    private static List<UsbDevice> findQhyDevices(UsbManager manager) {
        HashMap<String, UsbDevice> deviceList = manager.getDeviceList();
        List<UsbDevice> devices = new ArrayList<>();
        for (UsbDevice device : deviceList.values()) {
            if (device.getVendorId() == VENDOR_QHYCCD) {
                devices.add(device);
            }
        }
        Collections.sort(devices, Comparator.comparing(UsbDevice::getDeviceName));
        return devices;
    }

    private static String buildSummary(UsbManager manager, List<UsbDevice> devices) {
        StringBuilder builder = new StringBuilder();
        for (UsbDevice device : devices) {
            if (builder.length() > 0) {
                builder.append(" | ");
            }
            builder.append(device.getDeviceName())
                    .append(" vid=0x").append(Integer.toHexString(device.getVendorId()))
                    .append(" pid=0x").append(Integer.toHexString(device.getProductId()))
                    .append(" ifaces=").append(device.getInterfaceCount())
                    .append(" permission=").append(manager.hasPermission(device));
        }
        return builder.toString();
    }

    private static String buildSignature(List<UsbDevice> devices) {
        StringBuilder builder = new StringBuilder();
        for (UsbDevice device : devices) {
            if (builder.length() > 0) {
                builder.append('|');
            }
            builder.append(device.getDeviceName())
                    .append(':')
                    .append(device.getVendorId())
                    .append(':')
                    .append(device.getProductId());
        }
        return builder.toString();
    }

    private static void ensureReceiverRegistered(Context context) {
        if (receiverRegistered) {
            return;
        }

        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                String action = intent.getAction();
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                String deviceName = device != null ? device.getDeviceName() : "null";
                if (ACTION_USB_PERMISSION.equals(action)) {
                    boolean granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                    Log.i(TAG, "USB permission result: granted=" + granted + " device=" + deviceName);
                    return;
                }

                if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                    Log.i(TAG, "QHY USB attached: " + deviceName);
                } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                    Log.i(TAG, "QHY USB detached: " + deviceName);
                }
            }
        };

        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_USB_PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        if (Build.VERSION.SDK_INT >= 33) {
            context.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            context.registerReceiver(receiver, filter);
        }
        receiverRegistered = true;
    }

    private static void requestPermission(Context context, UsbManager manager, UsbDevice usbDevice) {
        int flags = Build.VERSION.SDK_INT >= 23 ? PendingIntent.FLAG_IMMUTABLE : 0;
        PendingIntent permissionIntent = PendingIntent.getBroadcast(
                context,
                usbDevice.getDeviceId(),
                new Intent(ACTION_USB_PERMISSION),
                flags);
        manager.requestPermission(usbDevice, permissionIntent);
    }
}

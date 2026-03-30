package org.qtproject.example.QUARCS_app;

import android.app.PendingIntent;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.util.Log;

import org.qtproject.qt.android.QtNative;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Locale;

public final class UsbSerialBridge {
    private static final String TAG = "UsbSerialBridge";
    private static final String ACTION_USB_PERMISSION = "org.qtproject.example.QUARCS_app.USB_PERMISSION";
    private static final int VENDOR_QINHENG = 0x1a86;
    private static final int PRODUCT_CH340 = 0x7523;
    private static final int PRODUCT_CH341A = 0x5523;

    private static final Object LOCK = new Object();

    private static UsbDeviceConnection connection;
    private static UsbDevice device;
    private static UsbInterface controlInterface;
    private static UsbInterface dataInterface;
    private static UsbEndpoint inEndpoint;
    private static UsbEndpoint outEndpoint;
    private static String lastError = "";
    private static boolean receiverRegistered = false;

    private UsbSerialBridge() {
    }

    public static boolean open(String preferredPath, int baudRate) {
        synchronized (LOCK) {
            closeLocked();

            Activity activity = QtNative.activity();
            if (activity == null) {
                lastError = "Android activity is null";
                return false;
            }

            Context context = activity;
            UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
            if (manager == null) {
                lastError = "UsbManager is null";
                return false;
            }

            ensureReceiverRegistered(context);

            UsbDevice selectedDevice = findSupportedDevice(manager, preferredPath);
            if (selectedDevice == null) {
                lastError = "No supported USB serial device found";
                Log.w(TAG, lastError + ". Devices: " + describeDevices(manager));
                return false;
            }

            if (!manager.hasPermission(selectedDevice)) {
                requestPermission(context, manager, selectedDevice);
                lastError = "USB permission requested for " + selectedDevice.getDeviceName() + ". Retry after granting permission.";
                Log.w(TAG, lastError);
                return false;
            }

            UsbInterface[] pair = findInterfaces(selectedDevice);
            if (pair[0] == null || pair[1] == null) {
                lastError = "Serial interfaces not found on " + selectedDevice.getDeviceName();
                Log.w(TAG, lastError);
                return false;
            }

            UsbEndpoint[] endpoints = findEndpoints(pair[1]);
            if (endpoints[0] == null || endpoints[1] == null) {
                lastError = "Bulk endpoints not found on " + selectedDevice.getDeviceName();
                Log.w(TAG, lastError);
                return false;
            }

            UsbDeviceConnection openedConnection = manager.openDevice(selectedDevice);
            if (openedConnection == null) {
                lastError = "UsbManager.openDevice returned null for " + selectedDevice.getDeviceName();
                Log.w(TAG, lastError);
                return false;
            }

            for (int i = 0; i < selectedDevice.getInterfaceCount(); i++) {
                UsbInterface usbInterface = selectedDevice.getInterface(i);
                if (!openedConnection.claimInterface(usbInterface, true)) {
                    lastError = "Failed to claim USB interface " + i;
                    Log.w(TAG, lastError);
                    openedConnection.close();
                    return false;
                }
            }

            if (!configureConnection(selectedDevice, openedConnection, pair[0], baudRate)) {
                releaseClaimedInterfaces(selectedDevice, openedConnection);
                openedConnection.close();
                return false;
            }

            connection = openedConnection;
            device = selectedDevice;
            controlInterface = pair[0];
            dataInterface = pair[1];
            inEndpoint = endpoints[0];
            outEndpoint = endpoints[1];
            lastError = "";

            Log.i(TAG, "Opened USB serial device " + device.getDeviceName()
                    + " vid=0x" + Integer.toHexString(device.getVendorId())
                    + " pid=0x" + Integer.toHexString(device.getProductId())
                    + " baud=" + baudRate
                    + " preferredPath=" + preferredPath);
            return true;
        }
    }

    public static void close() {
        synchronized (LOCK) {
            closeLocked();
        }
    }

    public static boolean isOpen() {
        synchronized (LOCK) {
            return connection != null && device != null && inEndpoint != null && outEndpoint != null;
        }
    }

    public static boolean write(byte[] payload, int timeoutMs) {
        synchronized (LOCK) {
            if (!isOpen()) {
                lastError = "USB serial bridge is not open";
                return false;
            }

            int totalWritten = 0;
            while (totalWritten < payload.length) {
                byte[] chunk = Arrays.copyOfRange(payload, totalWritten, payload.length);
                int written = connection.bulkTransfer(outEndpoint, chunk, chunk.length, timeoutMs);
                if (written <= 0) {
                    lastError = "bulkTransfer write failed: " + written;
                    Log.w(TAG, lastError);
                    return false;
                }
                totalWritten += written;
            }

            lastError = "";
            return true;
        }
    }

    public static byte[] read(int timeoutMs) {
        synchronized (LOCK) {
            if (!isOpen()) {
                lastError = "USB serial bridge is not open";
                return new byte[0];
            }

            byte[] buffer = new byte[512];
            int read = connection.bulkTransfer(inEndpoint, buffer, buffer.length, timeoutMs);
            if (read <= 0) {
                return new byte[0];
            }

            lastError = "";
            return Arrays.copyOf(buffer, read);
        }
    }

    public static String getLastError() {
        synchronized (LOCK) {
            return lastError;
        }
    }

    public static String describeConnectedDevices() {
        Activity activity = QtNative.activity();
        if (activity == null) {
            return "activity=null";
        }

        Context context = activity;
        UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        if (manager == null) {
            return "usbManager=null";
        }

        return describeDevices(manager);
    }

    private static void closeLocked() {
        if (connection != null) {
            try {
                releaseClaimedInterfaces(device, connection);
            } catch (Exception ignored) {
            }
            connection.close();
        }

        connection = null;
        device = null;
        controlInterface = null;
        dataInterface = null;
        inEndpoint = null;
        outEndpoint = null;
    }

    private static void ensureReceiverRegistered(Context context) {
        if (receiverRegistered) {
            return;
        }

        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                if (!ACTION_USB_PERMISSION.equals(intent.getAction())) {
                    return;
                }

                UsbDevice grantedDevice = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                boolean granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                Log.i(TAG, "USB permission result: granted=" + granted
                        + " device=" + (grantedDevice != null ? grantedDevice.getDeviceName() : "null"));
            }
        };

        IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
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
                0,
                new Intent(ACTION_USB_PERMISSION),
                flags);
        manager.requestPermission(usbDevice, permissionIntent);
    }

    private static UsbDevice findSupportedDevice(UsbManager manager, String preferredPath) {
        HashMap<String, UsbDevice> deviceList = manager.getDeviceList();
        UsbDevice fallback = null;

        for (UsbDevice candidate : deviceList.values()) {
            if (!isSupportedSerial(candidate)) {
                continue;
            }

            if (fallback == null) {
                fallback = candidate;
            }

            if (preferredPath != null && !preferredPath.trim().isEmpty()) {
                String lowerPath = preferredPath.toLowerCase(Locale.US);
                String lowerName = candidate.getDeviceName().toLowerCase(Locale.US);
                if (lowerName.contains(lowerPath)) {
                    return candidate;
                }
            }
        }

        return fallback;
    }

    private static boolean isSupportedSerial(UsbDevice usbDevice) {
        if (isCh34xDevice(usbDevice)) {
            return usbDevice.getInterfaceCount() >= 1;
        }
        UsbInterface[] pair = findInterfaces(usbDevice);
        return pair[0] != null && pair[1] != null;
    }

    private static UsbInterface[] findInterfaces(UsbDevice usbDevice) {
        UsbInterface control = null;
        UsbInterface data = null;

        for (int i = 0; i < usbDevice.getInterfaceCount(); i++) {
            UsbInterface usbInterface = usbDevice.getInterface(i);
            if (usbInterface.getInterfaceClass() == UsbConstants.USB_CLASS_COMM && control == null) {
                control = usbInterface;
            } else if (usbInterface.getInterfaceClass() == UsbConstants.USB_CLASS_CDC_DATA && data == null) {
                data = usbInterface;
            }
        }

        if (control == null && usbDevice.getInterfaceCount() == 1) {
            UsbInterface only = usbDevice.getInterface(0);
            if (only.getEndpointCount() >= 2) {
                control = only;
                data = only;
            }
        }

        return new UsbInterface[] { control, data };
    }

    private static UsbEndpoint[] findEndpoints(UsbInterface usbInterface) {
        UsbEndpoint in = null;
        UsbEndpoint out = null;

        for (int i = 0; i < usbInterface.getEndpointCount(); i++) {
            UsbEndpoint endpoint = usbInterface.getEndpoint(i);
            if (endpoint.getType() != UsbConstants.USB_ENDPOINT_XFER_BULK) {
                continue;
            }

            if (endpoint.getDirection() == UsbConstants.USB_DIR_IN && in == null) {
                in = endpoint;
            } else if (endpoint.getDirection() == UsbConstants.USB_DIR_OUT && out == null) {
                out = endpoint;
            }
        }

        return new UsbEndpoint[] { in, out };
    }

    private static boolean configureConnection(UsbDevice usbDevice, UsbDeviceConnection openedConnection,
                                               UsbInterface control, int baudRate) {
        if (isCh34xDevice(usbDevice)) {
            return configureCh34x(openedConnection, baudRate);
        }

        byte[] lineCoding = new byte[] {
                (byte) (baudRate & 0xff),
                (byte) ((baudRate >> 8) & 0xff),
                (byte) ((baudRate >> 16) & 0xff),
                (byte) ((baudRate >> 24) & 0xff),
                0,
                0,
                8
        };

        int setLineCoding = openedConnection.controlTransfer(0x21, 0x20, 0, control.getId(), lineCoding, lineCoding.length, 2000);
        if (setLineCoding < 0) {
            lastError = "SET_LINE_CODING failed";
            Log.w(TAG, lastError);
            return false;
        }

        int setControlLineState = openedConnection.controlTransfer(0x21, 0x22, 0x0003, control.getId(), null, 0, 2000);
        if (setControlLineState < 0) {
            lastError = "SET_CONTROL_LINE_STATE failed";
            Log.w(TAG, lastError);
            return false;
        }

        return true;
    }

    private static boolean isCh34xDevice(UsbDevice usbDevice) {
        return usbDevice != null
                && usbDevice.getVendorId() == VENDOR_QINHENG
                && (usbDevice.getProductId() == PRODUCT_CH340 || usbDevice.getProductId() == PRODUCT_CH341A);
    }

    private static void releaseClaimedInterfaces(UsbDevice usbDevice, UsbDeviceConnection openedConnection) {
        if (usbDevice == null || openedConnection == null) {
            return;
        }

        for (int i = 0; i < usbDevice.getInterfaceCount(); i++) {
            try {
                openedConnection.releaseInterface(usbDevice.getInterface(i));
            } catch (Exception ignored) {
            }
        }
    }

    private static boolean configureCh34x(UsbDeviceConnection openedConnection, int baudRate) {
        try {
            ch34xCheckState(openedConnection, "init #1", 0x5f, 0, new int[] { -1, 0x00 });

            if (ch34xControlOut(openedConnection, 0xa1, 0, 0) < 0) {
                throw new IllegalStateException("CH34x init failed #2");
            }

            if (!ch34xSetBaudRate(openedConnection, baudRate)) {
                throw new IllegalStateException("CH34x unsupported baud rate " + baudRate);
            }

            ch34xCheckState(openedConnection, "init #4", 0x95, 0x2518, new int[] { -1, 0x00 });

            if (ch34xControlOut(openedConnection, 0x9a, 0x2518, 0x0050) < 0) {
                throw new IllegalStateException("CH34x init failed #5");
            }

            ch34xCheckState(openedConnection, "init #6", 0x95, 0x0706, new int[] { 0xff, 0xee });

            if (ch34xControlOut(openedConnection, 0xa1, 0x501f, 0xd90a) < 0) {
                throw new IllegalStateException("CH34x init failed #7");
            }

            if (!ch34xSetBaudRate(openedConnection, baudRate)) {
                throw new IllegalStateException("CH34x unsupported baud rate " + baudRate);
            }

            if (ch34xControlOut(openedConnection, 0xa4, ~0, 0) < 0) {
                throw new IllegalStateException("CH34x handshake setup failed");
            }

            ch34xCheckState(openedConnection, "init #10", 0x95, 0x0706, new int[] { -1, 0xee });
            return true;
        } catch (Exception error) {
            lastError = error.getMessage();
            Log.w(TAG, lastError);
            return false;
        }
    }

    private static int ch34xControlOut(UsbDeviceConnection openedConnection, int request, int value, int index) {
        return openedConnection.controlTransfer(0x41, request, value, index, null, 0, 2000);
    }

    private static int ch34xControlIn(UsbDeviceConnection openedConnection, int request, int value, int index, byte[] buffer) {
        return openedConnection.controlTransfer(0xC0, request, value, index, buffer, buffer.length, 2000);
    }

    private static void ch34xCheckState(UsbDeviceConnection openedConnection, String label, int request, int value,
                                        int[] expected) {
        byte[] buffer = new byte[expected.length];
        int ret = ch34xControlIn(openedConnection, request, value, 0, buffer);
        if (ret < 0) {
            throw new IllegalStateException("CH34x " + label + " read failed");
        }
        if (ret != expected.length) {
            throw new IllegalStateException("CH34x " + label + " returned " + ret + " bytes");
        }

        for (int i = 0; i < expected.length; i++) {
            if (expected[i] == -1) {
                continue;
            }
            int current = buffer[i] & 0xff;
            if (current != expected[i]) {
                throw new IllegalStateException("CH34x " + label + " expected 0x"
                        + Integer.toHexString(expected[i]) + " got 0x" + Integer.toHexString(current));
            }
        }
    }

    private static boolean ch34xSetBaudRate(UsbDeviceConnection openedConnection, int baudRate) {
        int[] baud = new int[] {
                2400, 0xd901, 0x0038,
                4800, 0x6402, 0x001f,
                9600, 0xb202, 0x0013,
                19200, 0xd902, 0x000d,
                38400, 0x6403, 0x000a,
                115200, 0xcc03, 0x0008
        };

        for (int i = 0; i < baud.length; i += 3) {
            if (baud[i] != baudRate) {
                continue;
            }

            if (ch34xControlOut(openedConnection, 0x9a, 0x1312, baud[i + 1]) < 0) {
                lastError = "CH34x set baud stage #1 failed";
                Log.w(TAG, lastError);
                return false;
            }
            if (ch34xControlOut(openedConnection, 0x9a, 0x0f2c, baud[i + 2]) < 0) {
                lastError = "CH34x set baud stage #2 failed";
                Log.w(TAG, lastError);
                return false;
            }
            return true;
        }

        lastError = "CH34x baud rate not supported: " + baudRate;
        Log.w(TAG, lastError);
        return false;
    }

    private static String describeDevices(UsbManager manager) {
        StringBuilder builder = new StringBuilder();
        for (UsbDevice candidate : manager.getDeviceList().values()) {
            if (builder.length() > 0) {
                builder.append(" | ");
            }

            builder.append(candidate.getDeviceName())
                    .append(" vid=0x").append(Integer.toHexString(candidate.getVendorId()))
                    .append(" pid=0x").append(Integer.toHexString(candidate.getProductId()))
                    .append(" ifaces=").append(candidate.getInterfaceCount())
                    .append(" permission=").append(manager.hasPermission(candidate));
        }
        return builder.toString();
    }
}

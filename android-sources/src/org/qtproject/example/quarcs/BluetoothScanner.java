package org.qtproject.example.quarcs;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class BluetoothScanner {
    private static final String TAG = "BluetoothScanner";
    private static long qtObjectPtr = 0;
    
    private Context context;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bluetoothLeScanner;
    private ScanCallback scanCallback;
    private Handler handler;
    private boolean isScanning = false;
    
    // 服务 UUID，与 MyApplication 中的一致
    private static final UUID SERVICE_UUID = UUID.fromString("00001234-0000-1000-8000-00805f9b34fb");
    
    public BluetoothScanner(Context context) {
        Log.d(TAG, "BluetoothScanner: Constructor called, context=" + (context != null ? "valid" : "null"));
        this.context = context;
        this.handler = new Handler(Looper.getMainLooper());
        Log.d(TAG, "BluetoothScanner: Handler created");
        
        BluetoothManager bluetoothManager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        Log.d(TAG, "BluetoothScanner: BluetoothManager=" + (bluetoothManager != null ? "valid" : "null"));
        if (bluetoothManager != null) {
            bluetoothAdapter = bluetoothManager.getAdapter();
            Log.d(TAG, "BluetoothScanner: BluetoothAdapter=" + (bluetoothAdapter != null ? "valid" : "null"));
            if (bluetoothAdapter != null) {
                bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
                Log.d(TAG, "BluetoothScanner: BluetoothLeScanner=" + (bluetoothLeScanner != null ? "valid" : "null"));
            }
        }
        
        initScanCallback();
        Log.d(TAG, "BluetoothScanner: Constructor completed");
    }
    
    public static void setQtObject(long ptr) {
        Log.d(TAG, "BluetoothScanner: setQtObject called, ptr=" + ptr);
        qtObjectPtr = ptr;
        Log.d(TAG, "BluetoothScanner: setQtObject completed, qtObjectPtr=" + qtObjectPtr);
    }
    
    public static void setCallbacks(long ptr) {
        Log.d(TAG, "BluetoothScanner: setCallbacks called, ptr=" + ptr);
        qtObjectPtr = ptr;
        Log.d(TAG, "BluetoothScanner: setCallbacks completed, qtObjectPtr=" + qtObjectPtr);
    }
    
    private void initScanCallback() {
        scanCallback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                if (!isScanning) return;
                
                BluetoothDevice device = result.getDevice();
                String deviceName = device.getName();
                String deviceAddress = device.getAddress();
                
                if (deviceName == null) {
                    deviceName = "";
                }
                
                Log.d(TAG, "Found device: " + deviceName + " (" + deviceAddress + ")");
                
                // 通过 JNI 回调到 C++
                if (qtObjectPtr != 0) {
                    Log.d(TAG, "BluetoothScanner: Calling onDeviceFoundNative, ptr=" + qtObjectPtr);
                    try {
                        onDeviceFoundNative(qtObjectPtr, deviceName, deviceAddress);
                        Log.d(TAG, "BluetoothScanner: onDeviceFoundNative returned successfully");
                    } catch (Exception e) {
                        Log.e(TAG, "BluetoothScanner: Exception in onDeviceFoundNative", e);
                    }
                } else {
                    Log.w(TAG, "BluetoothScanner: qtObjectPtr is 0, cannot call native method");
                }
            }
            
            @Override
            public void onScanFailed(int errorCode) {
                isScanning = false;
                String error = "Scan failed with error code: " + errorCode;
                Log.e(TAG, error);
                
                if (qtObjectPtr != 0) {
                    onScanErrorNative(qtObjectPtr, error);
                }
            }
        };
    }
    
    public void startScan(String timeoutStr) {
        Log.d(TAG, "BluetoothScanner: startScan called, timeoutStr=" + timeoutStr);
        Log.d(TAG, "BluetoothScanner: isScanning=" + isScanning + ", qtObjectPtr=" + qtObjectPtr);
        
        if (isScanning) {
            Log.d(TAG, "BluetoothScanner: Scan already in progress");
            return;
        }
        
        Log.d(TAG, "BluetoothScanner: Checking permissions...");
        if (!checkPermissions()) {
            String error = "Bluetooth permissions not granted";
            Log.e(TAG, "BluetoothScanner: " + error);
            if (qtObjectPtr != 0) {
                Log.d(TAG, "BluetoothScanner: Calling onScanErrorNative, ptr=" + qtObjectPtr);
                onScanErrorNative(qtObjectPtr, error);
            }
            return;
        }
        Log.d(TAG, "BluetoothScanner: Permissions OK");
        
        Log.d(TAG, "BluetoothScanner: Checking BluetoothAdapter...");
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            String error = "Bluetooth adapter not available or not enabled";
            Log.e(TAG, "BluetoothScanner: " + error);
            if (qtObjectPtr != 0) {
                Log.d(TAG, "BluetoothScanner: Calling onScanErrorNative, ptr=" + qtObjectPtr);
                onScanErrorNative(qtObjectPtr, error);
            }
            return;
        }
        Log.d(TAG, "BluetoothScanner: BluetoothAdapter OK");
        
        Log.d(TAG, "BluetoothScanner: Checking BluetoothLeScanner...");
        if (bluetoothLeScanner == null) {
            String error = "Bluetooth LE Scanner not available";
            Log.e(TAG, "BluetoothScanner: " + error);
            if (qtObjectPtr != 0) {
                Log.d(TAG, "BluetoothScanner: Calling onScanErrorNative, ptr=" + qtObjectPtr);
                onScanErrorNative(qtObjectPtr, error);
            }
            return;
        }
        Log.d(TAG, "BluetoothScanner: BluetoothLeScanner OK");
        
        isScanning = true;
        Log.d(TAG, "BluetoothScanner: Creating scan settings...");
        
        // 配置扫描设置
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build();
        Log.d(TAG, "BluetoothScanner: ScanSettings created");
        
        // 设置扫描过滤器（按服务 UUID）
        List<ScanFilter> filters = new ArrayList<>();
        ScanFilter filter = new ScanFilter.Builder()
                .setServiceUuid(new ParcelUuid(SERVICE_UUID))
                .build();
        filters.add(filter);
        Log.d(TAG, "BluetoothScanner: ScanFilter created");
        
        try {
            Log.d(TAG, "BluetoothScanner: About to call bluetoothLeScanner.startScan()...");
            bluetoothLeScanner.startScan(filters, settings, scanCallback);
            Log.d(TAG, "BluetoothScanner: bluetoothLeScanner.startScan() called successfully");
            
            // 设置超时
            int timeout = Integer.parseInt(timeoutStr);
            Log.d(TAG, "BluetoothScanner: Setting timeout to " + timeout + "ms");
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    Log.d(TAG, "BluetoothScanner: Timeout reached, stopping scan");
                    if (isScanning) {
                        stopScan();
                    }
                }
            }, timeout);
            Log.d(TAG, "BluetoothScanner: startScan completed successfully");
        } catch (SecurityException e) {
            isScanning = false;
            String error = "Security exception: " + e.getMessage();
            Log.e(TAG, "BluetoothScanner: " + error, e);
            if (qtObjectPtr != 0) {
                Log.d(TAG, "BluetoothScanner: Calling onScanErrorNative after exception, ptr=" + qtObjectPtr);
                onScanErrorNative(qtObjectPtr, error);
            }
        } catch (Exception e) {
            isScanning = false;
            String error = "Exception in startScan: " + e.getMessage();
            Log.e(TAG, "BluetoothScanner: " + error, e);
            if (qtObjectPtr != 0) {
                Log.d(TAG, "BluetoothScanner: Calling onScanErrorNative after exception, ptr=" + qtObjectPtr);
                onScanErrorNative(qtObjectPtr, error);
            }
        }
    }
    
    public void stopScan() {
        if (!isScanning || bluetoothLeScanner == null) {
            return;
        }
        
        try {
            bluetoothLeScanner.stopScan(scanCallback);
            isScanning = false;
            Log.d(TAG, "Bluetooth scan stopped");
            
            if (qtObjectPtr != 0) {
                onScanFinishedNative(qtObjectPtr);
            }
        } catch (SecurityException e) {
            Log.e(TAG, "Security exception while stopping scan: " + e.getMessage());
        }
    }
    
    private boolean checkPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                   context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        } else {
            return context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
        }
    }
    
    // JNI 方法声明
    private native void onDeviceFoundNative(long ptr, String name, String address);
    private native void onScanFinishedNative(long ptr);
    private native void onScanErrorNative(long ptr, String error);
    
    static {
        Log.d(TAG, "BluetoothScanner: Loading native library QUARCS_app...");
        try {
            System.loadLibrary("QUARCS_app");
            Log.d(TAG, "BluetoothScanner: Native library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "BluetoothScanner: Failed to load native library", e);
        }
    }
}


#pragma once

#include <windows.h>
#include <bluetoothapis.h>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

struct BluetoothDevice {
    std::wstring name;
    BLUETOOTH_ADDRESS address;
    bool connected;
    bool authenticated; // Paired
    int rssi;
    unsigned long cod; // Class of Device
};

class DeviceScanner {
public:
    DeviceScanner();
    ~DeviceScanner();

    // Starts async scanning
    void StartScanning();
    void StopScanning();

    std::vector<BluetoothDevice> GetDiscoveredDevices();
    
    // Callback for new device found
    void SetOnDeviceFoundCallback(std::function<void(const BluetoothDevice&)> callback);

private:
    void ScanLoop();

    bool scanning_;
    std::mutex mutex_;
    std::vector<BluetoothDevice> cached_devices_;
    std::function<void(const BluetoothDevice&)> on_device_found_;
    HANDLE scan_thread_;
};

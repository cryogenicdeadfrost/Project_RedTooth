#define NOMINMAX
#include "DeviceScanner.h"
#include <thread>
#include <iostream>
#include <ws2bth.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cstdarg> // For va_list, va_start, va_end
#include <cstdio> // For vprintf, printf
#include <chrono> // For std::chrono::milliseconds

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "Ws2_32.lib")

// Helper for CLI logging
void LogCLI(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

DeviceScanner::DeviceScanner() : scanning_(false), scan_thread_(nullptr), on_device_found_(nullptr) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

DeviceScanner::~DeviceScanner() {
    StopScanning();
}

bool DeviceScanner::StartScanning() {
    std::lock_guard<std::mutex> lock(mutex_); // Protect scanning state change
    if (scanning_) return true;

    // 1. Validate Radio Logic
    if (!IsValidRadio()) {
        LogCLI("[ERROR] Bluetooth Radio is not ready or not connectable/discoverable.");
        return false;
    }
    
    LogCLI("[INFO] Starting device scanning...");
    
    scanning_ = true;
    scan_thread_ = CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
        static_cast<DeviceScanner*>(param)->ScanLoop();
        return 0;
    }, this, 0, NULL);
    
    if (!scan_thread_) {
        DWORD error = GetLastError();
        scanning_ = false;
        LogCLI("[ERROR] CreateThread failed: %lu", error);
        return false;
    }
    
    LogCLI("[INFO] Scan thread created successfully");
    return true;
}

void DeviceScanner::StopScanning() {
    // Determine if we need to stop
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!scanning_) return;
        scanning_ = false;
    }

    // Wait outside the lock to avoid deadlock if ScanLoop tries to acquire it
    if (scan_thread_) {
        LogCLI("[INFO] Waiting for scan thread to stop...");
        WaitForSingleObject(scan_thread_, INFINITE);
        CloseHandle(scan_thread_);
        scan_thread_ = nullptr;
        LogCLI("[INFO] Scan thread stopped.");
    }
}

std::vector<BluetoothDevice> DeviceScanner::GetDiscoveredDevices() {
    std::lock_guard<std::mutex> lock(mutex_);
    return cached_devices_;
}

void DeviceScanner::SetOnDeviceFoundCallback(std::function<void(const BluetoothDevice&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_device_found_ = callback;
}

bool DeviceScanner::IsValidRadio() {
    BLUETOOTH_FIND_RADIO_PARAMS radioParams = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
    HANDLE hRadio = NULL;
    HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio(&radioParams, &hRadio);

    if (hFind) {
        BluetoothFindRadioClose(hFind);
        if (hRadio) {
            BLUETOOTH_RADIO_INFO radioInfo = { sizeof(BLUETOOTH_RADIO_INFO) };
            DWORD ret = BluetoothGetRadioInfo(hRadio, &radioInfo);
            CloseHandle(hRadio);
            
            if (ret == ERROR_SUCCESS) {
                // LogCLI("[INFO] Radio Found: %S", radioInfo.szName);
                if (BluetoothIsConnectable(hRadio) && BluetoothIsDiscoverable(hRadio)) {
                     return true;
                } else {
                    // LogCLI("[WARN] Radio found but not Connectable/Discoverable.");
                    // In strict mode this might return false, but often "Connectable" is enough.
                    // For now, if we found a radio, we assume we can try to scan.
                    return true; 
                }
            }
        }
    }
    return false; // No radio handle means no access or no hardware
}

void DeviceScanner::ScanLoop() {
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams;
    ZeroMemory(&searchParams, sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS));
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnUnknown = TRUE;
    searchParams.fReturnConnected = TRUE;
    searchParams.fIssueInquiry = TRUE;
    searchParams.cTimeoutMultiplier = 4; // ~5 seconds
    searchParams.hRadio = NULL;

    BLUETOOTH_DEVICE_INFO deviceInfo;
    ZeroMemory(&deviceInfo, sizeof(BLUETOOTH_DEVICE_INFO));
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    
    LogCLI("[INFO] ScanLoop started");

    int consecutive_errors = 0;
    const int max_backoff_ms = 10000;
    int current_backoff_ms = 1000;
    
    while (true) {
        // Check scanning status safely
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!scanning_) break;
        }

        LogCLI("[INFO] Scanning cycle starting...");

        HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        
        if (hFind) {
            consecutive_errors = 0;
            current_backoff_ms = 1000;
            
            int device_count = 0;
            do {
                BluetoothDevice dev;
                dev.name = deviceInfo.szName;
                dev.address = deviceInfo.Address;
                dev.connected = deviceInfo.fConnected;
                dev.authenticated = deviceInfo.fAuthenticated;
                dev.cod = deviceInfo.ulClassofDevice;
                dev.rssi = 0; // Windows API doesn't give RSSI easily in this struct without Winsock

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    bool exists = false;
                    for (auto& existing : cached_devices_) {
                        if (existing.address.ullLong == dev.address.ullLong) {
                            existing.name = dev.name; // Update name
                            existing.connected = dev.connected;
                            existing.authenticated = dev.authenticated;
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        cached_devices_.push_back(dev);
                        // LogCLI("[CLI] Device Found: %S", dev.name.c_str()); // C++ CLI Echo
                        if (on_device_found_) {
                            on_device_found_(dev);
                        }
                        device_count++;
                    }
                }
            } while (BluetoothFindNextDevice(hFind, &deviceInfo));
            
            BluetoothFindDeviceClose(hFind);
            // LogCLI("[INFO] Scan cycle matched %d devices.", device_count);
        } else {
            int err = GetLastError();
            if (err == ERROR_NO_MORE_ITEMS) {
                // No devices found, this is normal
            } else {
                consecutive_errors++;
                LogCLI("[WARN] BluetoothFindFirstDevice failed or empty. Error: %d", err);
                
                if (consecutive_errors > 2) {
                     current_backoff_ms = (std::min)(current_backoff_ms * 2, max_backoff_ms);
                     // Add jitter (±20%)
                     int jitter = (rand() % (current_backoff_ms / 5)) - (current_backoff_ms / 10);
                     current_backoff_ms = (std::max)(1000, current_backoff_ms + jitter);
                }
            }
        }
        
        // Sleep interruptible? 
        // For simplicity, sleep in chunks
        for (int i = 0; i < current_backoff_ms; i += 100) {
             std::lock_guard<std::mutex> lock(mutex_);
             if (!scanning_) break;
             std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Use std::this_thread::sleep_for
        }
    }
    
    LogCLI("[INFO] ScanLoop exited.");
}

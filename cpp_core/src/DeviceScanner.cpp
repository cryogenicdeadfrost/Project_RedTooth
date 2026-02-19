#include "DeviceScanner.h"
#include <thread>
#include <iostream>
#include <ws2bth.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "Ws2_32.lib")

DeviceScanner::DeviceScanner() : scanning_(false), scan_thread_(nullptr), on_device_found_(nullptr) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

DeviceScanner::~DeviceScanner() {
    StopScanning();
}

bool DeviceScanner::StartScanning() {
    if (scanning_) return true;
    
    FILE* log = fopen("bt_debug.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] Starting device scanning...\n"); 
        fclose(log); 
    }
    
    scanning_ = true;
    scan_thread_ = CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
        static_cast<DeviceScanner*>(param)->ScanLoop();
        return 0;
    }, this, 0, NULL);
    
    if (!scan_thread_) {
        DWORD error = GetLastError();
        scanning_ = false;
        
        log = fopen("bt_debug.txt", "a");
        if (log) { 
            fprintf(log, "[ERROR] CreateThread failed: %d\n", error); 
            fclose(log); 
        }
        return false;
    }
    
    log = fopen("bt_debug.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] Scan thread created successfully\n"); 
        fclose(log); 
    }
    
    return true;
}

void DeviceScanner::StopScanning() {
    scanning_ = false;
    if (scan_thread_) {
        WaitForSingleObject(scan_thread_, INFINITE);
        CloseHandle(scan_thread_);
        scan_thread_ = nullptr;
    }
}

std::vector<BluetoothDevice> DeviceScanner::GetDiscoveredDevices() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BluetoothDevice> copy = cached_devices_;
    return copy;
}

void DeviceScanner::SetOnDeviceFoundCallback(DeviceFoundCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_device_found_ = callback;
}

void DeviceScanner::ScanLoop() {
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {
        sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),
        1, // fReturnAuthenticated
        1, // fReturnRemembered
        1, // fReturnUnknown
        1, // fIssueInquiry
        2, // cTimeoutMultiplier
        NULL
    };

    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(BLUETOOTH_DEVICE_INFO), 0 };
    
    FILE* log = fopen("bt_debug.txt", "w");
    if (log) { 
        fprintf(log, "[INFO] ScanLoop started\n"); 
        fclose(log); 
    }

    int consecutive_errors = 0;
    const int max_backoff_ms = 30000; // 30 seconds max backoff
    int current_backoff_ms = 2000; // Start with 2 seconds
    
    while (scanning_) {
        log = fopen("bt_debug.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] Starting BluetoothFindFirstDevice... (backoff: %dms)\n", current_backoff_ms); 
            fclose(log); 
        }

        HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        
        if (hFind) {
            consecutive_errors = 0;
            current_backoff_ms = 2000; // Reset to normal interval
            
            log = fopen("bt_debug.txt", "a");
            if (log) { 
                fprintf(log, "[INFO] Device found: first\n"); 
                fclose(log); 
            }
            
            int device_count = 0;
            do {
                BluetoothDevice dev;
                dev.name = deviceInfo.szName;
                dev.address = deviceInfo.Address;
                dev.connected = deviceInfo.fConnected;
                dev.authenticated = deviceInfo.fAuthenticated;
                dev.cod = deviceInfo.ulClassofDevice;
                dev.rssi = 0; 

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    bool exists = false;
                    for (auto& existing : cached_devices_) {
                        if (existing.address.ullLong == dev.address.ullLong) {
                            existing = dev; // Update
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        cached_devices_.push_back(dev);
                        if (on_device_found_) {
                            on_device_found_(dev);
                        }
                        device_count++;
                    }
                }
            } while (BluetoothFindNextDevice(hFind, &deviceInfo));
            
            BluetoothFindDeviceClose(hFind);
            
            log = fopen("bt_debug.txt", "a");
            if (log) { 
                fprintf(log, "[INFO] Found %d new devices in this scan cycle\n", device_count); 
                fclose(log); 
            }
        } else {
            int err = GetLastError();
            consecutive_errors++;
            
            // Exponential backoff with jitter
            if (consecutive_errors > 1) {
                current_backoff_ms = std::min(current_backoff_ms * 2, max_backoff_ms);
                // Add jitter (±20%)
                int jitter = (rand() % (current_backoff_ms / 5)) - (current_backoff_ms / 10);
                current_backoff_ms = std::max(1000, current_backoff_ms + jitter);
            }
            
            log = fopen("bt_debug.txt", "a");
            if (log) { 
                fprintf(log, "[ERROR] BluetoothFindFirstDevice failed. Error: %d (consecutive errors: %d, next backoff: %dms)\n", 
                        err, consecutive_errors, current_backoff_ms); 
                fclose(log); 
            }
            
            // Log specific error messages for common errors
            if (err == 160) { // ERROR_BAD_DEVICE
                log = fopen("bt_debug.txt", "a");
                if (log) { 
                    fprintf(log, "[WARN] Error 160: Bluetooth radio may be turned off or not available\n"); 
                    fclose(log); 
                }
            } else if (err == 1168) { // ERROR_NOT_FOUND
                log = fopen("bt_debug.txt", "a");
                if (log) { 
                    fprintf(log, "[INFO] Error 1168: No Bluetooth devices found in range\n"); 
                    fclose(log); 
                }
            }
        }
        
        log = fopen("bt_debug.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] Scan cycle finished. Sleeping for %dms...\n", current_backoff_ms); 
            fclose(log); 
        }
        
        // Wait with backoff before next scan cycle
        for (int i = 0; i < current_backoff_ms && scanning_; i += 100) {
            Sleep(100);
        }
    }
}

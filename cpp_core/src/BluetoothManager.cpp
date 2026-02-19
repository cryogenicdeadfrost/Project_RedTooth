#include "BluetoothManager.h"
#include "DeviceScanner.h"
#include "ConnectionPool.h"
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <sstream>

// Global singleton instances for simplicity in this FFI layer
static std::unique_ptr<DeviceScanner> g_scanner;
static std::unique_ptr<ConnectionPool> g_pool;

// Error handling
static std::string g_last_bt_error;
static std::string g_last_audio_error;
static std::mutex g_error_mutex;
static OnErrorCallback g_error_callback = nullptr;

// Helper function to set error and call error callback
static void set_error(const std::string& error, std::string& target_error, FfiErrorCode code = FFI_OPERATION_FAILED) {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    target_error = error;
    
    if (g_error_callback) {
        g_error_callback(code, target_error.c_str());
    }
    
    // Log to file for debugging
    FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
    if (log) { 
        fprintf(log, "[ERROR] %s (code: %d)\n", error.c_str(), code); 
        fclose(log); 
    }
}

FfiErrorCode bt_init(OnErrorCallback error_callback) {
    g_error_callback = error_callback;
    
    FILE* log = fopen("bt_debug_mgr_v2.txt", "w");
    if (log) { 
        fprintf(log, "[INFO] bt_init called\n"); 
        fclose(log); 
    }

    try {
        g_scanner = std::make_unique<DeviceScanner>();
        g_pool = std::make_unique<ConnectionPool>();
        
        log = fopen("bt_debug_mgr_v2.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] BluetoothManager initialized successfully\n"); 
            fclose(log); 
        }
        
        return FFI_SUCCESS;
    } catch (const std::exception& e) {
        set_error(std::string("Failed to initialize BluetoothManager: ") + e.what(), g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    } catch (...) {
        set_error("Unknown exception during BluetoothManager initialization", g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    }
}

// Need a global callback to bridge C to C++ raw pointer callback
static OnDeviceFoundCallback g_c_callback = nullptr;

void DeviceFoundTrampoline(const BluetoothDevice& dev) {
    if (!g_c_callback) return;
    
    DiscoveredDevice c_dev;
    c_dev.address = dev.address.ullLong;
    
    // Convert wstring to string (UTF-8)
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &dev.name[0], (int)dev.name.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte(CP_UTF8, 0, &dev.name[0], (int)dev.name.size(), &strTo[0], size_needed, NULL, NULL);
    
    c_dev.name = strTo.c_str(); 
    c_dev.connected = dev.connected;
    c_dev.authenticated = dev.authenticated;
    c_dev.rssi = dev.rssi;
    c_dev.cod = dev.cod;
    
    g_c_callback(c_dev);
}

FfiErrorCode bt_start_scan(OnDeviceFoundCallback callback, OnErrorCallback error_callback) {
    if (!g_scanner) {
        set_error("Bluetooth not initialized", g_last_bt_error, FFI_NOT_INITIALIZED);
        return FFI_NOT_INITIALIZED;
    }
    
    g_c_callback = callback;
    g_error_callback = error_callback;
    g_scanner->SetOnDeviceFoundCallback(DeviceFoundTrampoline);
    
    try {
        if (!g_scanner->StartScanning()) {
            set_error("Failed to start scanning", g_last_bt_error, FFI_OPERATION_FAILED);
            return FFI_OPERATION_FAILED;
        }
        
        FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] bt_start_scan called successfully\n"); 
            fclose(log); 
        }
        
        return FFI_SUCCESS;
    } catch (const std::exception& e) {
        set_error(std::string("Failed to start scan: ") + e.what(), g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    } catch (...) {
        set_error("Unknown exception during scan start", g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    }
}

FfiErrorCode bt_stop_scan() {
    if (!g_scanner) {
        set_error("Bluetooth not initialized", g_last_bt_error, FFI_NOT_INITIALIZED);
        return FFI_NOT_INITIALIZED;
    }
    
    try {
        g_scanner->StopScanning();
        
        FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] bt_stop_scan called successfully\n"); 
            fclose(log); 
        }
        
        return FFI_SUCCESS;
    } catch (const std::exception& e) {
        set_error(std::string("Failed to stop scan: ") + e.what(), g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    } catch (...) {
        set_error("Unknown exception during scan stop", g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    }
}

FfiErrorCode bt_connect_device(unsigned long long address) {
    if (!g_pool) {
        set_error("Connection pool not initialized", g_last_bt_error, FFI_NOT_INITIALIZED);
        return FFI_NOT_INITIALIZED;
    }
    
    BLUETOOTH_ADDRESS addr;
    addr.ullLong = address;
    
    try {
        if (!g_pool->ConnectDevice(addr)) {
            set_error("Failed to connect to device", g_last_bt_error, FFI_CONNECTION_FAILED);
            return FFI_CONNECTION_FAILED;
        }
        
        FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] Connected to device: %llu\n", address); 
            fclose(log); 
        }
        
        return FFI_SUCCESS;
    } catch (const std::exception& e) {
        set_error(std::string("Connection failed: ") + e.what(), g_last_bt_error, FFI_CONNECTION_FAILED);
        return FFI_CONNECTION_FAILED;
    } catch (...) {
        set_error("Unknown exception during connection", g_last_bt_error, FFI_CONNECTION_FAILED);
        return FFI_CONNECTION_FAILED;
    }
}

FfiErrorCode bt_disconnect_device(unsigned long long address) {
    if (!g_pool) {
        set_error("Connection pool not initialized", g_last_bt_error, FFI_NOT_INITIALIZED);
        return FFI_NOT_INITIALIZED;
    }
    
    BLUETOOTH_ADDRESS addr;
    addr.ullLong = address;
    
    try {
        if (!g_pool->DisconnectDevice(addr)) {
            set_error("Failed to disconnect from device", g_last_bt_error, FFI_OPERATION_FAILED);
            return FFI_OPERATION_FAILED;
        }
        
        FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] Disconnected from device: %llu\n", address); 
            fclose(log); 
        }
        
        return FFI_SUCCESS;
    } catch (const std::exception& e) {
        set_error(std::string("Disconnection failed: ") + e.what(), g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    } catch (...) {
        set_error("Unknown exception during disconnection", g_last_bt_error, FFI_OPERATION_FAILED);
        return FFI_OPERATION_FAILED;
    }
}

const char* bt_get_last_error() {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    return g_last_bt_error.c_str();
}

// Audio functions (stubs for now)
FfiErrorCode audio_init(OnErrorCallback error_callback) {
    g_error_callback = error_callback;
    
    FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] audio_init called\n"); 
        fclose(log); 
    }
    
    // TODO: Implement actual audio initialization
    return FFI_SUCCESS;
}

FfiErrorCode audio_start() {
    FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] audio_start called\n"); 
        fclose(log); 
    }
    
    // TODO: Implement actual audio start
    return FFI_SUCCESS;
}

FfiErrorCode audio_stop() {
    FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] audio_stop called\n"); 
        fclose(log); 
    }
    
    // TODO: Implement actual audio stop
    return FFI_SUCCESS;
}

FfiErrorCode audio_add_device(unsigned long long address) {
    FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] audio_add_device called for address: %llu\n", address); 
        fclose(log); 
    }
    
    // TODO: Implement actual audio device addition
    return FFI_SUCCESS;
}

FfiErrorCode audio_remove_device(unsigned long long address) {
    FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] audio_remove_device called for address: %llu\n", address); 
        fclose(log); 
    }
    
    // TODO: Implement actual audio device removal
    return FFI_SUCCESS;
}

const char* audio_get_last_error() {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    return g_last_audio_error.c_str();
}

int audio_get_channel_count(unsigned long long address) {
    // TODO: Implement actual channel count lookup
    // For now, return a mock value
    FILE* log = fopen("bt_debug_mgr_v2.txt", "a");
    if (log) { 
        fprintf(log, "[INFO] audio_get_channel_count called for address: %llu\n", address); 
        fclose(log); 
    }
    
    // Mock: Return 2 channels for even addresses, 1 for odd
    return (address % 2 == 0) ? 2 : 1;
}

bool bt_check_permission() {
    BLUETOOTH_FIND_RADIO_PARAMS params;
    params.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);
    
    HANDLE hRadio = NULL;
    HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio(&params, &hRadio);
    
    bool hasPermission = true;
    
    if (hFind) {
        // Found at least one radio, so we have permission
        BluetoothFindRadioClose(hFind);
        if (hRadio) CloseHandle(hRadio);
        
        FILE* log = fopen("bt_debug_mgr.txt", "a");
        if (log) { 
            fprintf(log, "[INFO] bt_check_permission: Radio found, permission granted\n"); 
            fclose(log); 
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            hasPermission = false;
            FILE* log = fopen("bt_debug_mgr.txt", "a");
            if (log) { 
                fprintf(log, "[ERROR] bt_check_permission: Access Denied (Error %lu)\n", error); 
                fclose(log); 
            }
        } else {
            // Other errors (e.g. no radio found) usually imply we had permission to look
            FILE* log = fopen("bt_debug_mgr.txt", "a");
            if (log) { 
                fprintf(log, "[INFO] bt_check_permission: No radio found but access allowed (Error %lu)\n", error); 
                fclose(log); 
            }
        }
    }
    
    return hasPermission;
}

#include "ConnectionPool.h"
#include "ProfileManager.h"
#include <iostream>

ConnectionPool::ConnectionPool() {
    // No explicit Winsock startup needed if we only use BluetoothAPIs, 
    // but good practice if we use sockets later.
}

ConnectionPool::~ConnectionPool() {
}

bool ConnectionPool::ConnectDevice(BLUETOOTH_ADDRESS address) {
    if (IsConnected(address)) return true;

    bool success = ProfileManager::EnableAudioSink(address);
    if (success) {
        std::lock_guard<std::mutex> lock(mutex_);
        // In a real app we'd get a handle or some token, but for high-level link manager:
        active_connections_[address.ullLong] = (HANDLE)1; 
    }
    return success;
}

bool ConnectionPool::DisconnectDevice(BLUETOOTH_ADDRESS address) {
    bool success = ProfileManager::DisableAudioSink(address);
    if (success) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_connections_.erase(address.ullLong);
    }
    return success;
}

bool ConnectionPool::IsConnected(BLUETOOTH_ADDRESS address) {
    // Check our map first
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_connections_.find(address.ullLong) == active_connections_.end()) {
            return false;
        }
    }

    // Verify with OS
    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };
    deviceInfo.Address = address;
    DWORD ret = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (ret == ERROR_SUCCESS) {
        return deviceInfo.fConnected;
    }
    return false;
}

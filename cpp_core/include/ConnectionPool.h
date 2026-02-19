#pragma once

#include <windows.h>
#include <bluetoothapis.h>
#include <vector>
#include <map>
#include <mutex>

class ConnectionPool {
public:
    ConnectionPool();
    ~ConnectionPool();

    bool ConnectDevice(BLUETOOTH_ADDRESS address);
    bool DisconnectDevice(BLUETOOTH_ADDRESS address);
    bool IsConnected(BLUETOOTH_ADDRESS address);

private:
    std::map<unsigned long long, HANDLE> active_connections_; // Address -> Socket/Handle
    std::mutex mutex_;
};

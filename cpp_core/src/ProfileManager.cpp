#include "ProfileManager.h"
#include <iostream>
#include <ws2bth.h>

// GUIDs for profiles
static GUID A2DP_SINK_GUID = { 0x0000110B, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB } };

bool ProfileManager::EnableAudioSink(const BLUETOOTH_ADDRESS& address) {
    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };
    deviceInfo.Address = address;
    
    DWORD ret = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (ret != ERROR_SUCCESS) {
        std::cerr << "Failed to get device info: " << ret << std::endl;
        return false;
    }

    // This call triggers the Windows Bluetooth stack to connect the profile
    ret = BluetoothSetServiceState(NULL, &deviceInfo, &A2DP_SINK_GUID, BLUETOOTH_SERVICE_ENABLE);
    if (ret != ERROR_SUCCESS) {
        std::cerr << "Failed to enable A2DP: " << ret << std::endl;
        return false;
    }
    
    return true;
}

bool ProfileManager::DisableAudioSink(const BLUETOOTH_ADDRESS& address) {
    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };
    deviceInfo.Address = address;

    DWORD ret = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (ret != ERROR_SUCCESS) return false;

    ret = BluetoothSetServiceState(NULL, &deviceInfo, &A2DP_SINK_GUID, BLUETOOTH_SERVICE_DISABLE);
    return (ret == ERROR_SUCCESS);
}

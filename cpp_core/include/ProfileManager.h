#pragma once

#include <windows.h>
#include <bluetoothapis.h>
#include <vector>

class ProfileManager {
public:
    static bool EnableAudioSink(const BLUETOOTH_ADDRESS& address);
    static bool DisableAudioSink(const BLUETOOTH_ADDRESS& address);
    
    // Future: HFP, AVRCP
};

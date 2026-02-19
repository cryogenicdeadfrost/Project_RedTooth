#pragma once

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#endif

    typedef struct {
        unsigned long long address;
        const char* name;
        bool connected;
        bool authenticated;
        int rssi;
        unsigned long cod;
    } DiscoveredDevice;

    // Error codes for FFI operations
    typedef enum {
        FFI_SUCCESS = 0,
        FFI_NOT_INITIALIZED = 1,
        FFI_INVALID_PARAMETER = 2,
        FFI_OPERATION_FAILED = 3,
        FFI_DEVICE_NOT_FOUND = 4,
        FFI_CONNECTION_FAILED = 5,
        FFI_AUDIO_INIT_FAILED = 6,
        FFI_UNKNOWN_ERROR = 255,
    } FfiErrorCode;

    // Callback types
    typedef void (*OnDeviceFoundCallback)(DiscoveredDevice device);
    typedef void (*OnErrorCallback)(FfiErrorCode error_code, const char* message);

    // Bluetooth functions
    FfiErrorCode bt_init(OnErrorCallback error_callback);
    FfiErrorCode bt_start_scan(OnDeviceFoundCallback callback, OnErrorCallback error_callback);
    FfiErrorCode bt_stop_scan();
    FfiErrorCode bt_connect_device(unsigned long long address);
    FfiErrorCode bt_disconnect_device(unsigned long long address);
    
    // Audio functions
    FfiErrorCode audio_init(OnErrorCallback error_callback);
    FfiErrorCode audio_start();
    FfiErrorCode audio_stop();
    FfiErrorCode audio_add_device(unsigned long long address);
    FfiErrorCode audio_remove_device(unsigned long long address);
    
    // Error handling
    const char* bt_get_last_error();
    const char* audio_get_last_error();
    
    // Audio info
    int audio_get_channel_count(unsigned long long address);

    // Permission check
    bool bt_check_permission();

#ifdef __cplusplus
}
#endif

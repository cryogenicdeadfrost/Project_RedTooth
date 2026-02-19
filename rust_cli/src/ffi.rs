use std::os::raw::{c_char, c_int};

#[repr(C)]
pub struct DiscoveredDevice {
    pub address: u64,
    pub name: *const c_char,
    pub connected: bool,
    pub authenticated: bool,
    pub rssi: c_int,
    pub cod: u32,
}

// Error codes for FFI operations
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum FfiErrorCode {
    Success = 0,
    NotInitialized = 1,
    InvalidParameter = 2,
    OperationFailed = 3,
    DeviceNotFound = 4,
    ConnectionFailed = 5,
    AudioInitFailed = 6,
    UnknownError = 255,
}

// Callback types
pub type OnDeviceFoundCallback = extern "C" fn(device: DiscoveredDevice);
pub type OnErrorCallback = extern "C" fn(error_code: FfiErrorCode, message: *const c_char);

// #[link(name = "bt_core", kind = "static")]
extern "C" {
    // Bluetooth
    pub fn bt_init(error_callback: OnErrorCallback) -> FfiErrorCode;
    pub fn bt_start_scan(callback: OnDeviceFoundCallback, error_callback: OnErrorCallback) -> FfiErrorCode;
    pub fn bt_stop_scan() -> FfiErrorCode;
    pub fn bt_connect_device(address: u64) -> FfiErrorCode;
    pub fn bt_disconnect_device(address: u64) -> FfiErrorCode;
    
    // Audio
    pub fn audio_init(error_callback: OnErrorCallback) -> FfiErrorCode;
    pub fn audio_start() -> FfiErrorCode;
    pub fn audio_stop() -> FfiErrorCode;
    pub fn audio_add_device(address: u64) -> FfiErrorCode;
    pub fn audio_remove_device(address: u64) -> FfiErrorCode;
    
    // Error handling
    pub fn bt_get_last_error() -> *const c_char;
    pub fn audio_get_last_error() -> *const c_char;
    
    // Audio info
    pub fn audio_get_channel_count(address: u64) -> c_int;
    
    // Permission check
    pub fn bt_check_permission() -> bool;
}

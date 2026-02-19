use crate::error::{AppError, Result};
use crate::ffi;
use std::ffi::CStr;
use std::sync::{Arc, Mutex};
use log::{error, info, warn};

#[derive(Clone, Debug)]
pub struct BluetoothDevice {
    pub address: u64,
    pub name: String,
    pub connected: bool,
    pub authenticated: bool,
    pub rssi: i32,
    pub cod: u32,
}

// Global state for callback to verify/update
lazy_static::lazy_static! {
    pub static ref DISCOVERED_DEVICES: Arc<Mutex<Vec<BluetoothDevice>>> = Arc::new(Mutex::new(Vec::new()));
    static ref LAST_ERROR: Arc<Mutex<String>> = Arc::new(Mutex::new(String::new()));
}

extern "C" fn on_device_found(device: ffi::DiscoveredDevice) {
    let name = unsafe {
        if device.name.is_null() {
            String::new()
        } else {
            CStr::from_ptr(device.name).to_string_lossy().into_owned()
        }
    };

    let dev = BluetoothDevice {
        address: device.address,
        name,
        connected: device.connected,
        authenticated: device.authenticated,
        rssi: device.rssi,
        cod: device.cod,
    };

    info!("Device found: {} ({})", dev.name, dev.address);
    
    let list = DISCOVERED_DEVICES.clone();
    match list.lock() {
        Ok(mut devices) => {
            let existing = devices.iter_mut().find(|d| d.address == dev.address);
            if let Some(entry) = existing {
                *entry = dev.clone();
                info!("Updated existing device: {}", dev.address);
            } else {
                devices.push(dev.clone());
                info!("Added new device: {} ({})", dev.name, dev.address);
            }
        }
        Err(poisoned) => {
            error!("Mutex poisoned while updating device list: {:?}", poisoned);
            // Try to recover by getting the poisoned data
            let mut devices = poisoned.into_inner();
            devices.push(dev);
        }
    };
}

extern "C" fn on_error(error_code: ffi::FfiErrorCode, message: *const std::os::raw::c_char) {
    let error_msg = unsafe {
        if message.is_null() {
            format!("Error code: {:?}", error_code)
        } else {
            format!("Error {:?}: {}", error_code, CStr::from_ptr(message).to_string_lossy())
        }
    };
    
    match error_code {
        ffi::FfiErrorCode::Success => info!("FFI operation successful"),
        _ => error!("FFI error: {}", error_msg),
    }
    
    // Store the last error
    if let Ok(mut error) = LAST_ERROR.lock() {
        *error = error_msg;
    }
}

pub fn init() -> Result<()> {
    info!("Initializing Bluetooth...");
    let result = unsafe { ffi::bt_init(on_error) };
    
    match result {
        ffi::FfiErrorCode::Success => {
            info!("Bluetooth initialized successfully");
            Ok(())
        }
        _ => {
            let error_msg = get_last_error();
            error!("Failed to initialize Bluetooth: {}", error_msg);
            Err(AppError::bluetooth(&error_msg))
        }
    }
}

pub fn start_scan() -> Result<()> {
    info!("Starting Bluetooth scan...");
    let result = unsafe { ffi::bt_start_scan(on_device_found, on_error) };
    
    match result {
        ffi::FfiErrorCode::Success => {
            info!("Bluetooth scan started successfully");
            Ok(())
        }
        _ => {
            let error_msg = get_last_error();
            error!("Failed to start Bluetooth scan: {}", error_msg);
            Err(AppError::bluetooth(&error_msg))
        }
    }
}

pub fn stop_scan() -> Result<()> {
    info!("Stopping Bluetooth scan...");
    let result = unsafe { ffi::bt_stop_scan() };
    
    match result {
        ffi::FfiErrorCode::Success => {
            info!("Bluetooth scan stopped successfully");
            Ok(())
        }
        _ => {
            let error_msg = get_last_error();
            error!("Failed to stop Bluetooth scan: {}", error_msg);
            Err(AppError::bluetooth(&error_msg))
        }
    }
}

pub fn connect(address: u64) -> Result<()> {
    info!("Connecting to device: {}", address);
    let result = unsafe { ffi::bt_connect_device(address) };
    
    match result {
        ffi::FfiErrorCode::Success => {
            info!("Successfully connected to device: {}", address);
            Ok(())
        }
        ffi::FfiErrorCode::ConnectionFailed => {
            let error_msg = get_last_error();
            error!("Connection failed for device {}: {}", address, error_msg);
            Err(AppError::bluetooth(&format!("Connection failed: {}", error_msg)))
        }
        _ => {
            let error_msg = get_last_error();
            error!("Failed to connect to device {}: {}", address, error_msg);
            Err(AppError::bluetooth(&error_msg))
        }
    }
}

pub fn disconnect(address: u64) -> Result<()> {
    info!("Disconnecting from device: {}", address);
    let result = unsafe { ffi::bt_disconnect_device(address) };
    
    match result {
        ffi::FfiErrorCode::Success => {
            info!("Successfully disconnected from device: {}", address);
            Ok(())
        }
        _ => {
            let error_msg = get_last_error();
            error!("Failed to disconnect from device {}: {}", address, error_msg);
            Err(AppError::bluetooth(&error_msg))
        }
    }
}

pub fn get_discovered_devices() -> Result<Vec<BluetoothDevice>> {
    let list = DISCOVERED_DEVICES.clone();
    match list.lock() {
        Ok(devices) => {
            info!("Retrieved {} discovered devices", devices.len());
            Ok(devices.to_vec())
        }
        Err(poisoned) => {
            error!("Mutex poisoned while getting device list");
            // Try to recover
            let devices = poisoned.into_inner();
            warn!("Recovered {} devices from poisoned mutex", devices.len());
            Ok(devices)
        }
    }
}

        }
    }
}

pub fn check_permission() -> bool {
    // Check if we have permission to access Bluetooth radio
    unsafe { ffi::bt_check_permission() }
}

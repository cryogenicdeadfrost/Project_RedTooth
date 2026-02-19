use crate::error::{AppError, Result};
use crate::ffi;
use std::ffi::CStr;
use std::sync::{Arc, Mutex};
use std::sync::mpsc::{self, Receiver, Sender};
use log::{error, info};

// ---- Data Structures ----

#[derive(Clone, Debug)]
pub struct BluetoothDevice {
    pub address: u64,
    pub name: String,
    pub connected: bool,
    pub authenticated: bool,
    pub rssi: i32,
    pub cod: u32,
}

#[derive(Debug, Clone)]
pub enum BluetoothEvent {
    DeviceFound(BluetoothDevice),
    ScanStarted,
    ScanStopped,
    Connected(u64),
    Disconnected(u64),
    Error(String),
}

// ---- Global Channel State ----
// We use a global Sender so the C callback can access it.
// This Mutex is ONLY for the Sender, not the data. It is locked extremely briefly.
lazy_static::lazy_static! {
    static ref EVENT_SENDER: Mutex<Option<Sender<BluetoothEvent>>> = Mutex::new(None);
}

// ---- FFI Callbacks ----

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
        name: name.clone(),
        connected: device.connected,
        authenticated: device.authenticated,
        rssi: device.rssi,
        cod: device.cod,
    };

    // CLI ECHO
    println!("CLI: Device Found: {} ({:X})", dev.name, dev.address);

    send_event(BluetoothEvent::DeviceFound(dev));
}

extern "C" fn on_error(error_code: ffi::FfiErrorCode, message: *const std::os::raw::c_char) {
    let error_msg = unsafe {
        if message.is_null() {
            format!("Error code: {:?}", error_code)
        } else {
            format!("Error {:?}: {}", error_code, CStr::from_ptr(message).to_string_lossy())
        }
    };
    
    // CLI ECHO
    if error_code != ffi::FfiErrorCode::Success {
        println!("CLI: FFI Error: {}", error_msg);
        send_event(BluetoothEvent::Error(error_msg));
    }
}

fn send_event(event: BluetoothEvent) {
    if let Ok(guard) = EVENT_SENDER.lock() {
        if let Some(sender) = &*guard {
            let _ = sender.send(event);
        }
    }
}

// ---- Public API ----

/// Initializes the Bluetooth subsystem and returns a Receiver for events.
/// This must be called once.
pub fn init() -> Result<Receiver<BluetoothEvent>> {
    println!("CLI: Initializing Bluetooth Manager...");
    info!("Initializing Bluetooth Manager...");

    let (tx, rx) = mpsc::channel();
    
    // Set the global sender
    {
        let mut guard = EVENT_SENDER.lock().unwrap();
        *guard = Some(tx);
    }

    let result = unsafe { ffi::bt_init(on_error) };
    
    match result {
        ffi::FfiErrorCode::Success => {
            println!("CLI: Bluetooth Initialized Successfully.");
            Ok(rx)
        }
        _ => {
            println!("CLI: Failed to Initialize Bluetooth.");
            Err(AppError::bluetooth("Failed to initialize C++ core"))
        }
    }
}

pub fn start_scan() -> Result<()> {
    println!("CLI: Action -> Start Scan");
    let result = unsafe { ffi::bt_start_scan(on_device_found, on_error) };
    if result == ffi::FfiErrorCode::Success {
        send_event(BluetoothEvent::ScanStarted);
        Ok(())
    } else {
        Err(AppError::bluetooth("Failed to start scan"))
    }
}

pub fn stop_scan() -> Result<()> {
    println!("CLI: Action -> Stop Scan");
    let result = unsafe { ffi::bt_stop_scan() };
    if result == ffi::FfiErrorCode::Success {
        send_event(BluetoothEvent::ScanStopped);
        Ok(())
    } else {
        Err(AppError::bluetooth("Failed to stop scan"))
    }
}

pub fn connect(address: u64) -> Result<()> {
    println!("CLI: Action -> Connect to {:X}", address);
    let result = unsafe { ffi::bt_connect_device(address) };
    match result {
        ffi::FfiErrorCode::Success => {
             // We don't get an async callback for connection in this simple FFI yet,
             // so we speculate/send event here or wait for next scan update.
             // For now, let's assume success triggers an event.
             send_event(BluetoothEvent::Connected(address));
             Ok(())
        }
        _ => Err(AppError::bluetooth("Connection failed"))
    }
}

pub fn disconnect(address: u64) -> Result<()> {
    println!("CLI: Action -> Disconnect from {:X}", address);
    let result = unsafe { ffi::bt_disconnect_device(address) };
    match result {
        ffi::FfiErrorCode::Success => {
             send_event(BluetoothEvent::Disconnected(address));
             Ok(())
        }
        _ => Err(AppError::bluetooth("Disconnect failed"))
    }
}

pub fn check_permission() -> bool {
    println!("CLI: Action -> Check Permissions");
    unsafe { ffi::bt_check_permission() }
}

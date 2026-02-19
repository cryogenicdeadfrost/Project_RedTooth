//#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")] // Hide console in release

mod error;
mod ffi;
mod bluetooth;
mod config;
mod registry;
mod gui;

use crate::error::{AppError, Result};
use eframe::egui;
use gui::BluetoothApp;
use log::{error, info, LevelFilter};

fn setup_logging() -> Result<()> {
    // Configure logging
    env_logger::Builder::new()
        .filter_level(LevelFilter::Info)
        .filter_module("btmanager", LevelFilter::Debug)
        .format_timestamp_secs()
        .format_module_path(false)
        .format_target(false)
        .init();
    
    info!("Logging initialized");
    Ok(())
}

fn initialize_application() -> Result<()> {
    println!("CHECKING_RUST_MAIN_EXECUTION");
    info!("Starting RedTooth Manager...");
    
    // Initialize Bluetooth
    match bluetooth::init() {
        Ok(_) => info!("Bluetooth initialized successfully"),
        Err(e) => {
            error!("Failed to initialize Bluetooth: {}", e);
            // Continue anyway - Bluetooth might not be available
        }
    }
    
    // Load configuration
    match config::Config::load() {
        Ok(config) => info!("Configuration loaded with {} devices", config.devices.len()),
        Err(e) => error!("Failed to load configuration: {}", e),
    }
    
    // Initialize registry
    match registry::Registry::new() {
        Ok(_) => info!("Registry initialized successfully"),
        Err(e) => error!("Failed to initialize registry: {}", e),
    }
    
    Ok(())
}

fn main() -> Result<()> {
    // Setup logging
    if let Err(e) = setup_logging() {
        eprintln!("Failed to setup logging: {}", e);
    }
    
    // Initialize application components
    if let Err(e) = initialize_application() {
        error!("Application initialization failed: {}", e);
        // Continue anyway - some components might still work
    }
    
    info!("Starting GUI...");
    
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([400.0, 600.0])
            .with_min_inner_size([300.0, 400.0])
            .with_title("RedTooth Manager - Bluetooth Device Manager"),
        ..Default::default()
    };
    
    eframe::run_native(
        "RedTooth Manager",
        options,
        Box::new(|cc| {
            // Set up GUI context
            cc.egui_ctx.set_visuals(egui::Visuals::dark());
            Box::new(BluetoothApp::new(cc))
        }),
    ).map_err(|e| {
        error!("GUI runtime error: {}", e);
        AppError::Gui(format!("GUI runtime error: {}", e))
    })
}

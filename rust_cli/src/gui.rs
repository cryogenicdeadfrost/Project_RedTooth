use crate::bluetooth::{self, BluetoothDevice};
use crate::config::Config;
use crate::error::AppError;
use crate::ffi;
use crate::registry::Registry;
use eframe::{egui, App, Frame};
use log::{error, info, warn};
use std::sync::{Arc, Mutex};
use std::time::Duration;

pub struct BluetoothApp {
    devices: Arc<Mutex<Vec<BluetoothDevice>>>,
    registry: Result<Registry, AppError>,
    config: Result<Config, AppError>,
    error_message: Option<String>,
    scanning: bool,
    permission_granted: bool,
}

impl BluetoothApp {
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        info!("Initializing BluetoothApp GUI...");
        
        // Load configuration
        let config = Config::load();
        if let Err(ref e) = config {
            error!("Failed to load config: {}", e);
        }
        
        // Initialize registry
        let registry = Registry::new();
        if let Err(ref e) = registry {
            error!("Failed to initialize registry: {}", e);
        }
        
        // Start Bluetooth scan
        let scanning = match bluetooth::start_scan() {
            Ok(_) => {
                info!("Bluetooth scan started successfully");
                true
            }
            Err(e) => {
                error!("Failed to start Bluetooth scan: {}", e);
                false
            }
        };
        
        // Check permissions
        let permission_granted = bluetooth::check_permission();
        if !permission_granted {
            error!("Bluetooth permission check failed: Access Denied or No Radio found");
        } else {
            info!("Bluetooth permission granted");
        }
        
        // Attempt auto-connect if config loaded successfully
        if let Ok(ref config) = config {
            if let Ok(ref registry) = registry {
                info!("Attempting auto-connect for {} devices", config.auto_connect.len());
                for name in &config.auto_connect {
                    if let Some(&addr) = config.devices.get(name) {
                        match bluetooth::connect(addr) {
                            Ok(_) => {
                                info!("Auto-connected to device: {} ({})", name, addr);
                                // Log to registry
                                if let Err(e) = registry.log_device(addr, name) {
                                    warn!("Failed to log auto-connected device to registry: {}", e);
                                }
                            }
                            Err(e) => {
                                warn!("Failed to auto-connect to device {} ({}): {}", name, addr, e);
                            }
                        }
                    }
                }
            }
        }
        
        Self {
            devices: bluetooth::DISCOVERED_DEVICES.clone(),
            registry,
            config,
            error_message: None,
            scanning,
            permission_granted,
        }
    }
    
    fn show_error_dialog(&mut self, ctx: &egui::Context, message: &str) {
        egui::Window::new("Error")
            .collapsible(false)
            .resizable(false)
            .anchor(egui::Align2::CENTER_CENTER, egui::Vec2::ZERO)
            .show(ctx, |ui| {
                ui.label(egui::RichText::new(message).color(egui::Color32::RED));
                ui.separator();
                if ui.button("OK").clicked() {
                    self.error_message = None;
                }
            });
    }
}

impl App for BluetoothApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut Frame) {
        // Continuous repaint for device updates
        ctx.request_repaint_after(Duration::from_millis(1000));

        // Show error dialog if there's an error message
        if let Some(error_msg) = self.error_message.clone() {
            self.show_error_dialog(ctx, &error_msg);
        }

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("RedTooth Manager - Bluetooth Device Manager");
            
            // Status bar
            ui.horizontal(|ui| {
                ui.label("Status:");
                if self.scanning {
                    ui.colored_label(egui::Color32::GREEN, "Scanning");
                } else {
                    ui.colored_label(egui::Color32::RED, "Not Scanning");
                }
                
                // Show config/registry status
                match (&self.config, &self.registry) {
                    (Ok(_), Err(_)) => ui.colored_label(egui::Color32::YELLOW, "⚠ Registry Error"),
                    (Err(_), Err(_)) => ui.colored_label(egui::Color32::RED, "✗ Config & Registry Error"),
                }
            });
            
            // Permission Warning
            if !self.permission_granted {
                ui.add_space(5.0);
                ui.group(|ui| {
                    ui.horizontal(|ui| {
                        ui.label(egui::RichText::new("⚠ PERMISSION DENIED").color(egui::Color32::RED).strong());
                        ui.label("Cannot access Bluetooth Radio. Please enable Bluetooth in OS settings.");
                         if ui.button("Check Again").clicked() {
                            self.permission_granted = bluetooth::check_permission();
                        }
                    });
                });
            }
            
            ui.separator();

            // Control buttons
            ui.horizontal(|ui| {
                if ui.button(if self.scanning { "Stop Scan" } else { "Start Scan" }).clicked() {
                    if self.scanning {
                        match bluetooth::stop_scan() {
                            Ok(_) => {
                                info!("Scan stopped successfully");
                                self.scanning = false;
                            }
                            Err(e) => {
                                error!("Failed to stop scan: {}", e);
                                self.error_message = Some(format!("Failed to stop scan: {}", e));
                            }
                        }
                    } else {
                        match bluetooth::start_scan() {
                            Ok(_) => {
                                info!("Scan started successfully");
                                self.scanning = true;
                            }
                            Err(e) => {
                                error!("Failed to start scan: {}", e);
                                self.error_message = Some(format!("Failed to start scan: {}", e));
                            }
                        }
                    }
                }
                
                if ui.button("Connect All").clicked() {
                    // Extract device addresses first to avoid borrowing issues
                    let device_addresses: Vec<u64> = match self.devices.lock() {
                        Ok(devices_guard) => devices_guard.iter().map(|d| d.address).collect(),
                        Err(e) => {
                            error!("Failed to lock device list: {:?}", e);
                            self.error_message = Some("Failed to access device list".to_string());
                            Vec::new()
                        }
                    };
                    
                    if !device_addresses.is_empty() {
                        let mut success_count = 0;
                        let mut error_count = 0;
                        
                        for address in device_addresses {
                            match bluetooth::connect(address) {
                                Ok(_) => success_count += 1,
                                Err(e) => {
                                    error!("Failed to connect to device {}: {}", address, e);
                                    error_count += 1;
                                }
                            }
                        }
                        
                        if error_count > 0 {
                            self.error_message = Some(format!("Connected {} devices, failed: {}", success_count, error_count));
                        } else {
                            info!("Successfully connected to all {} devices", success_count);
                        }
                    }
                }
                
                if ui.button("Disconnect All").clicked() {
                    // Extract device addresses first to avoid borrowing issues
                    let device_addresses: Vec<u64> = match self.devices.lock() {
                        Ok(devices_guard) => devices_guard.iter().map(|d| d.address).collect(),
                        Err(e) => {
                            error!("Failed to lock device list: {:?}", e);
                            self.error_message = Some("Failed to access device list".to_string());
                            Vec::new()
                        }
                    };
                    
                    if !device_addresses.is_empty() {
                        let mut success_count = 0;
                        let mut error_count = 0;
                        
                        for address in device_addresses {
                            match bluetooth::disconnect(address) {
                                Ok(_) => success_count += 1,
                                Err(e) => {
                                    error!("Failed to disconnect from device {}: {}", address, e);
                                    error_count += 1;
                                }
                            }
                        }
                        
                        if error_count > 0 {
                            self.error_message = Some(format!("Disconnected {} devices, failed: {}", success_count, error_count));
                        } else {
                            info!("Successfully disconnected from all {} devices", success_count);
                        }
                    }
                }
                
                // Refresh button
                if ui.button("Refresh").clicked() {
                    info!("Manual refresh requested");
                    ctx.request_repaint();
                }
            });

            ui.add_space(10.0);

            // Device count - extract device list first to avoid borrowing issues
            let device_list = match self.devices.lock() {
                Ok(devices_guard) => devices_guard.clone(),
                Err(e) => {
                    error!("Failed to lock device list: {:?}", e);
                    ui.colored_label(egui::Color32::RED, "Error: Failed to access device list");
                    Vec::new()
                }
            };
            
            ui.label(format!("Discovered Devices: {}", device_list.len()));
            ui.separator();

            // Device Grid
            if device_list.is_empty() {
                ui.vertical_centered(|ui| {
                    ui.add_space(20.0);
                    ui.label("No devices discovered yet");
                    ui.label("Make sure Bluetooth is enabled and click 'Start Scan'");
                    ui.add_space(20.0);
                });
            } else {
                egui::ScrollArea::vertical().show(ui, |ui| {
                    ui.vertical(|ui| {
                        for device in device_list.iter() {
                            self.draw_device_card(ui, device);
                        }
                    });
                });
            }
        });
    }

    fn on_exit(&mut self, _gl: Option<&eframe::glow::Context>) {
        info!("Application exiting, cleaning up...");
        
        // Stop scanning
        if self.scanning {
            if let Err(e) = bluetooth::stop_scan() {
                error!("Failed to stop scan on exit: {}", e);
            }
        }
        
        // Save config if it was loaded successfully
        if let Ok(ref config) = self.config {
            if let Err(e) = config.save() {
                error!("Failed to save config on exit: {}", e);
            }
        }
        
        info!("Application cleanup completed");
    }
}

impl BluetoothApp {
    fn draw_device_card(&mut self, ui: &mut egui::Ui, device: &BluetoothDevice) {
        ui.group(|ui| {
            ui.horizontal(|ui| {
                // Icon based on COD (Simplified logic)
                let icon = if device.cod & 0x200000 != 0 { "🎧" } // Audio
                          else if device.cod & 0x000400 != 0 { "📷" } // Camera/Imaging
                          else if device.cod & 0x000200 != 0 { "🖨️" } // Printer
                          else if device.cod & 0x000100 != 0 { "🖱️" } // Mouse/Keyboard
                          else { "📱" }; // Generic
                
                ui.label(egui::RichText::new(icon).size(24.0));
                
                ui.vertical(|ui| {
                    // Device name with address
                    ui.label(egui::RichText::new(&device.name).strong());
                    ui.small(format!("Address: {:X}", device.address));
                    
                    // Connection status with color
                    if device.connected {
                        ui.colored_label(egui::Color32::GREEN, "✓ Connected");
                    } else {
                        ui.colored_label(egui::Color32::GRAY, "✗ Disconnected");
                    }
                    
                    // Authentication status
                    if device.authenticated {
                        ui.small("🔒 Paired");
                    }
                    
                    // Audio Channel Info
                    let channels = unsafe { ffi::audio_get_channel_count(device.address) };
                    if device.connected && channels > 0 {
                        ui.small(format!("🎵 {} channel(s)", channels));
                    }
                    
                    // Show registry info if available
                    if let Ok(ref registry) = self.registry {
                        if let Ok(Some((_name, last_seen, count))) = registry.get_device_history(device.address) {
                            ui.small(format!("📊 Seen {} times, last: {}", count, last_seen));
                        }
                    }
                });

                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    // Connect/Disconnect button
                    if device.connected {
                        if ui.button("Disconnect").clicked() {
                            match bluetooth::disconnect(device.address) {
                                Ok(_) => info!("Disconnected from device: {}", device.address),
                                Err(e) => {
                                    error!("Failed to disconnect from device {}: {}", device.address, e);
                                    self.error_message = Some(format!("Failed to disconnect: {}", e));
                                }
                            }
                        }
                    } else {
                        if ui.button("Connect").clicked() {
                            match bluetooth::connect(device.address) {
                                Ok(_) => {
                                    info!("Connected to device: {}", device.address);
                                    // Log to registry if available
                                    if let Ok(ref registry) = self.registry {
                                        if let Err(e) = registry.log_device(device.address, &device.name) {
                                            warn!("Failed to log device to registry: {}", e);
                                        }
                                    }
                                }
                                Err(e) => {
                                    error!("Failed to connect to device {}: {}", device.address, e);
                                    self.error_message = Some(format!("Failed to connect: {}", e));
                                }
                            }
                        }
                    }
                    
                    // RSSI Bar with signal strength indicator
                    let rssi = device.rssi;
                    let rssi_norm = (rssi + 100).max(0).min(100) as f32 / 100.0;
                    let (color, strength) = if rssi >= -50 {
                        (egui::Color32::GREEN, "Excellent")
                    } else if rssi >= -60 {
                        (egui::Color32::LIGHT_GREEN, "Good")
                    } else if rssi >= -70 {
                        (egui::Color32::YELLOW, "Fair")
                    } else if rssi >= -80 {
                        (egui::Color32::from_rgb(255, 165, 0), "Weak") // Orange
                    } else {
                        (egui::Color32::RED, "Poor")
                    };
                    
                    ui.vertical(|ui| {
                        ui.add(egui::ProgressBar::new(rssi_norm)
                            .fill(color)
                            .desired_width(80.0)
                            .text(format!("{} dB", rssi)));
                        ui.small(strength);
                    });
                });
            });
        });
    }
}

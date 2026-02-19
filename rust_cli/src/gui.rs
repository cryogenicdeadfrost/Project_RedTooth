use crate::bluetooth::{self, BluetoothDevice, BluetoothEvent};
use crate::config::Config;
use crate::error::AppError;
use crate::ffi;
use crate::registry::Registry;
use eframe::{egui, App, Frame};
use log::{error, info, warn};
use std::sync::mpsc::Receiver;
use std::time::Duration;

pub struct BluetoothApp {
    // Devices are now owned by the GUI thread
    devices: Vec<BluetoothDevice>,
    // Channel to receive events from Bluetooth Manager
    event_receiver: Option<Receiver<BluetoothEvent>>,
    
    registry: Result<Registry, AppError>,
    config: Result<Config, AppError>,
    error_message: Option<String>,
    scanning: bool,
    permission_granted: bool,
}

impl BluetoothApp {
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        println!("CLI: GUI Initializing...");
        info!("Initializing BluetoothApp GUI...");
        
        // Load configuration
        let config = Config::load();
        
        // Initialize registry
        let registry = Registry::new();
        
        // Initialize Bluetooth Subsystem
        // This gives us the receiver for events
        let event_receiver = match bluetooth::init() {
            Ok(rx) => Some(rx),
            Err(e) => {
                error!("Failed to init bluetooth: {}", e);
                None
            }
        };

        // Check permissions
        let permission_granted = bluetooth::check_permission();
        println!("CLI: Permission Grant Status: {}", permission_granted);

        // Auto-start scan
        let scanning = if permission_granted {
            if let Ok(_) = bluetooth::start_scan() {
                true
            } else {
                false
            }
        } else {
            false
        };
        
        Self {
            devices: Vec::new(),
            event_receiver,
            registry,
            config,
            error_message: None,
            scanning,
            permission_granted,
        }
    }
    
    fn process_events(&mut self) {
        if let Some(rx) = &self.event_receiver {
            // Non-blocking loop to drain all pending events
            while let Ok(event) = rx.try_recv() {
                match event {
                    BluetoothEvent::DeviceFound(dev) => {
                        // println!("CLI: GUI Received Device: {}", dev.name); // Optional: verbose
                        
                        // Update or Add
                        if let Some(existing) = self.devices.iter_mut().find(|d| d.address == dev.address) {
                            *existing = dev;
                        } else {
                            self.devices.push(dev);
                        }
                    },
                    BluetoothEvent::ScanStarted => {
                        println!("CLI: GUI Event -> Scan Started");
                        self.scanning = true;
                    },
                    BluetoothEvent::ScanStopped => {
                        println!("CLI: GUI Event -> Scan Stopped");
                        self.scanning = false;
                    },
                    BluetoothEvent::Connected(addr) => {
                        println!("CLI: GUI Event -> Connected to {:X}", addr);
                        if let Some(d) = self.devices.iter_mut().find(|d| d.address == addr) {
                            d.connected = true;
                        }
                    },
                    BluetoothEvent::Disconnected(addr) => {
                        println!("CLI: GUI Event -> Disconnected from {:X}", addr);
                         if let Some(d) = self.devices.iter_mut().find(|d| d.address == addr) {
                            d.connected = false;
                        }
                    },
                    BluetoothEvent::Error(msg) => {
                        println!("CLI: GUI Event -> Error: {}", msg);
                        self.error_message = Some(msg);
                    }
                }
            }
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

    fn draw_device_card(&mut self, ui: &mut egui::Ui, device: &BluetoothDevice) {
        ui.group(|ui| {
            ui.horizontal(|ui| {
                ui.label(match device.cod {
                    // Simple heuristic for icons
                    c if c & 0x200000 != 0 => "🎧",
                    _ => "📱",
                });
                
                ui.vertical(|ui| {
                    ui.label(egui::RichText::new(&device.name).strong());
                    ui.small(format!("{:X}", device.address));
                    
                    if device.connected {
                        ui.colored_label(egui::Color32::GREEN, "Connected");
                    } else {
                        ui.label("Disconnected");
                    }
                });

                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if device.connected {
                        if ui.button("Disconnect").clicked() {
                             let _ = bluetooth::disconnect(device.address);
                        }
                    } else {
                        if ui.button("Connect").clicked() {
                             let _ = bluetooth::connect(device.address);
                        }
                    }
                     ui.label(format!("{} dB", device.rssi));
                });
                
            });
        });
    }
}

impl App for BluetoothApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut Frame) {
        // 1. Process Events
        self.process_events();
        
        ctx.request_repaint_after(Duration::from_millis(50)); // Responsive repaint

        // Show error dialog if there's an error message
        if let Some(error_msg) = self.error_message.clone() {
            self.show_error_dialog(ctx, &error_msg);
        }

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Project RedTooth");
            
            // Permission Warning
            if !self.permission_granted {
                ui.colored_label(egui::Color32::RED, "⚠ PERMISSION DENIED - Check OS Settings");
                if ui.button("Check Again").clicked() {
                    self.permission_granted = bluetooth::check_permission();
                }
            }

            ui.horizontal(|ui| {
                 if ui.button(if self.scanning { "Stop Scan" } else { "Start Scan" }).clicked() {
                     if self.scanning {
                         let _ = bluetooth::stop_scan();
                     } else {
                         let _ = bluetooth::start_scan();
                     }
                 }
                 
                 if ui.button("Clear List").clicked() {
                     println!("CLI: Action -> Clear List");
                     self.devices.clear();
                 }
            });
            
            ui.separator();

            egui::ScrollArea::vertical().show(ui, |ui| {
                 // We have to clone to iterate bc logging/drawing might mutate?
                 // Actually draw_device_card takes &mut self which is annoying if iterating self.devices.
                 // We will separate data from drawing method slightly or clone list.
                 // For now, let's just inline the draw logic or clone the device data to avoid borrow checker hell.
                 let items = self.devices.clone(); 
                 for device in items {
                     self.draw_device_card(ui, &device);
                 }
            });
        });
    }
}

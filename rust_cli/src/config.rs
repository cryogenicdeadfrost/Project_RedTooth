use crate::error::{AppError, Result};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use log::{info, warn, error};

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct Config {
    pub devices: HashMap<String, u64>, // Name -> Address
    pub auto_connect: Vec<String>, // List of names
}

impl Config {
    pub fn load() -> Result<Self> {
        let config_path = Path::new("config.toml");
        
        if !config_path.exists() {
            info!("Config file not found, using defaults");
            return Ok(Config::default());
        }
        
        info!("Loading config from {:?}", config_path);
        
        // Read file with error handling
        let content = match fs::read_to_string(config_path) {
            Ok(content) => content,
            Err(e) => {
                error!("Failed to read config file: {}", e);
                return Err(AppError::Io(e));
            }
        };
        
        // Parse TOML with error handling
        match toml::from_str::<Config>(&content) {
            Ok(config) => {
                info!("Config loaded successfully with {} devices", config.devices.len());
                Ok(config)
            }
            Err(e) => {
                error!("Failed to parse config file: {}", e);
                warn!("Using default config due to parse error");
                Ok(Config::default())
            }
        }
    }

    pub fn save(&self) -> Result<()> {
        info!("Saving config...");
        
        // Serialize to TOML
        let content = match toml::to_string(self) {
            Ok(content) => content,
            Err(e) => {
                error!("Failed to serialize config: {}", e);
                return Err(AppError::config(&format!("Serialization failed: {}", e)));
            }
        };
        
        // Write to file
        match fs::write("config.toml", content) {
            Ok(_) => {
                info!("Config saved successfully");
                Ok(())
            }
            Err(e) => {
                error!("Failed to write config file: {}", e);
                Err(AppError::Io(e))
            }
        }
    }
    
    pub fn add_device(&mut self, name: String, address: u64) {
        info!("Adding device: {} -> {}", name, address);
        self.devices.insert(name, address);
    }
    
    pub fn remove_device(&mut self, name: &str) -> bool {
        info!("Removing device: {}", name);
        self.devices.remove(name).is_some()
    }
    
    pub fn add_auto_connect(&mut self, name: String) {
        if !self.auto_connect.contains(&name) {
            info!("Adding {} to auto-connect list", name);
            self.auto_connect.push(name);
        }
    }
    
    pub fn remove_auto_connect(&mut self, name: &str) {
        info!("Removing {} from auto-connect list", name);
        self.auto_connect.retain(|n| n != name);
    }
}

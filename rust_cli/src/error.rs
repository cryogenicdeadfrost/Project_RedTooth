use thiserror::Error;

#[derive(Error, Debug)]
pub enum AppError {
    #[error("Database error: {0}")]
    Database(#[from] rusqlite::Error),
    
    #[error("Configuration error: {0}")]
    Config(String),
    
    #[error("File I/O error: {0}")]
    Io(#[from] std::io::Error),
    
    #[error("Bluetooth operation failed: {0}")]
    Bluetooth(String),
    
    #[error("Audio operation failed: {0}")]
    Audio(String),
    
    #[error("FFI error: {0}")]
    Ffi(String),
    
    #[error("GUI error: {0}")]
    Gui(String),
    
    #[error("Parse error: {0}")]
    Parse(String),
    
    #[error("Unknown error: {0}")]
    Unknown(String),
}

impl AppError {
    pub fn bluetooth(msg: &str) -> Self {
        AppError::Bluetooth(msg.to_string())
    }
    
    pub fn audio(msg: &str) -> Self {
        AppError::Audio(msg.to_string())
    }
    
    pub fn ffi(msg: &str) -> Self {
        AppError::Ffi(msg.to_string())
    }
    
    pub fn config(msg: &str) -> Self {
        AppError::Config(msg.to_string())
    }
}

pub type Result<T> = std::result::Result<T, AppError>;
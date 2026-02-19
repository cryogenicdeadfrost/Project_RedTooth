use crate::error::{AppError, Result};
use rusqlite::{params, Connection};
use std::path::Path;
use log::{info, warn, error};

pub struct Registry {
    conn: Connection,
}

impl Registry {
    pub fn new() -> Result<Self> {
        let path = Path::new("registry.db");
        info!("Opening registry database at {:?}", path);
        
        let conn = match Connection::open(path) {
            Ok(conn) => conn,
            Err(e) => {
                error!("Failed to open registry database: {}", e);
                return Err(AppError::Database(e));
            }
        };
        
        // Create table if it doesn't exist
        match conn.execute(
            "CREATE TABLE IF NOT EXISTS device_history (
                id INTEGER PRIMARY KEY,
                address INTEGER NOT NULL UNIQUE,
                name TEXT,
                last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
                connection_count INTEGER DEFAULT 0,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )",
            [],
        ) {
            Ok(_) => info!("Registry table created/verified"),
            Err(e) => {
                error!("Failed to create registry table: {}", e);
                return Err(AppError::Database(e));
            }
        }
        
        // Create index for faster lookups
        match conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_address ON device_history(address)",
            [],
        ) {
            Ok(_) => info!("Registry index created/verified"),
            Err(e) => warn!("Failed to create index (non-critical): {}", e),
        }
        
        Ok(Registry { conn })
    }

    pub fn log_device(&self, address: u64, name: &str) -> Result<()> {
        info!("Logging device to registry: {} ({})", name, address);
        
        // Use UPSERT (INSERT OR REPLACE) for simpler error handling
        match self.conn.execute(
            "INSERT OR REPLACE INTO device_history (address, name, last_seen, connection_count) 
             VALUES (?1, ?2, CURRENT_TIMESTAMP, 
                     COALESCE((SELECT connection_count + 1 FROM device_history WHERE address = ?1), 1))",
            params![address as i64, name],
        ) {
            Ok(_) => {
                info!("Device logged successfully: {} ({})", name, address);
                Ok(())
            }
            Err(e) => {
                error!("Failed to log device to registry: {}", e);
                Err(AppError::Database(e))
            }
        }
    }
    
    pub fn get_device_history(&self, address: u64) -> Result<Option<(String, String, i32)>> {
        match self.conn.query_row(
            "SELECT name, last_seen, connection_count FROM device_history WHERE address = ?1",
            params![address as i64],
            |row| {
                Ok((
                    row.get::<_, String>(0)?,
                    row.get::<_, String>(1)?,
                    row.get::<_, i32>(2)?,
                ))
            },
        ) {
            Ok(result) => {
                info!("Retrieved history for device: {}", address);
                Ok(Some(result))
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => {
                info!("No history found for device: {}", address);
                Ok(None)
            }
            Err(e) => {
                error!("Failed to get device history: {}", e);
                Err(AppError::Database(e))
            }
        }
    }
    
    pub fn get_all_devices(&self) -> Result<Vec<(u64, String, String, i32)>> {
        let mut stmt = match self.conn.prepare(
            "SELECT address, name, last_seen, connection_count FROM device_history ORDER BY last_seen DESC"
        ) {
            Ok(stmt) => stmt,
            Err(e) => {
                error!("Failed to prepare query for all devices: {}", e);
                return Err(AppError::Database(e));
            }
        };
        
        let device_iter = match stmt.query_map([], |row| {
            Ok((
                row.get::<_, i64>(0)? as u64,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, i32>(3)?,
            ))
        }) {
            Ok(iter) => iter,
            Err(e) => {
                error!("Failed to execute query for all devices: {}", e);
                return Err(AppError::Database(e));
            }
        };
        
        let mut devices = Vec::new();
        for device in device_iter {
            match device {
                Ok(device) => devices.push(device),
                Err(e) => {
                    error!("Failed to parse device row: {}", e);
                    return Err(AppError::Database(e));
                }
            }
        }
        
        info!("Retrieved {} devices from registry", devices.len());
        Ok(devices)
    }
    
    pub fn cleanup_old_entries(&self, days_old: i32) -> Result<usize> {
        info!("Cleaning up registry entries older than {} days", days_old);
        
        match self.conn.execute(
            "DELETE FROM device_history WHERE julianday('now') - julianday(last_seen) > ?1",
            params![days_old],
        ) {
            Ok(deleted) => {
                info!("Cleaned up {} old registry entries", deleted);
                Ok(deleted as usize)
            }
            Err(e) => {
                error!("Failed to cleanup registry: {}", e);
                Err(AppError::Database(e))
            }
        }
    }
}

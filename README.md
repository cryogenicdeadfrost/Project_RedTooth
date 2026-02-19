# Project RedTooth

RedTooth is a hybrid Bluetooth management utility for Windows, designed to split low-level API interactions from application logic and UI.

The core architecture uses a C++20 static library to interface with the Windows Bluetooth APIs (Win32/Bthprops), ensuring access to native functionality like device discovery, pairing status, and radio management. The frontend is built in Rust using `egui` for a lightweight, immediate-mode GUI, communicating with the core via a C-compatible FFI layer.

## Architecture

*   **cpp_core**: Static library implementing the `DeviceScanner`, `ConnectionPool`, and `BluetoothManager` classes. Handles thread-safe device enumeration and Win32 handle management.
*   **rust_cli**: Reference implementation of the frontend. Wraps the C++ FFI in unsafe Rust blocks and exposes a safe API to the UI layer. Uses SQLite for device persistence usage stats.

## Features

*   **Async Device Scanning**: threaded scanner with exponential backoff and jitter for reliability.
*   **FFI Bridge**: Clean separation of concerns; the C++ layer handles OS complexity, Rust handles safety and state.
*   **Permission Verification**: Explicit checks for radio access and OS-level permissions before attempting operations.
*   **Connection Pooling**: Manages active connections to prevent handle leaks.

## Build Instructions

Requirements:
*   Visual Studio Build Tools (MSVC)
*   CMake 3.10+
*   Rust (stable)

A PowerShell script is provided to orchestrate the build:

```powershell
.\build_all.ps1
```

This will:
1.  Configure and build the `bt_core` static lib using CMake.
2.  Link and build the Rust release binary via Cargo.
3.  Output the binary to `rust_cli/target/release/btmanager.exe`.

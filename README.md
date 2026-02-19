# 🦷 Project RedTooth

**RedTooth** is a high-performance Bluetooth Management System designed for Windows. It combines a robust **C++ Core** for low-level interactions with a modern, responsive **Rust GUI** for a seamless user experience.

## 🚀 Features

*   **⚡ Blazing Fast Scanning**: Quickly discover Bluetooth devices in your vicinity using our optimized C++ engine.
*   **🔗 Seamless Connection**: Connect and disconnect from devices with a single click. Support for auto-connection to your favorite devices.
*   **🎧 Audio Insights**: View advanced audio details like channel counts for connected audio devices.
*   **🛡️ Permission Aware**: Built-in system checks to ensure necessary Bluetooth permissions are granted, providing clear feedback if issues arise.
*   **📊 Device Registry**: Keeps a local history of your devices, tracking when they were last seen and how often you connect.

## 🛠️ Architecture

The project consists of two main components:

1.  **`cpp_core`**: A static library written in C++ (C++20) that handles the nitty-gritty of the Windows Bluetooth APIs (Win32).
2.  **`rust_cli`**: A Rust application that provides the GUI (using `egui`) and business logic, communicating with the core via FFI (Foreign Function Interface).

## 📦 Getting Started

### Prerequisites

*   **Windows 10/11**
*   **Rust Toolchain** (latest stable)
*   **C++ Build Tools** (Visual Studio / MSVC)
*   **CMake**

### Building the Project

We've made building easy with a single PowerShell script:

```powershell
.\build_all.ps1
```

This script will:
1.  Compile the C++ Core library.
2.  Build the Rust GUI application.
3.  Place the executable in `rust_cli/target/release/btmanager.exe`.

### Running

Once built, simply run the executable:

```powershell
.\rust_cli\target\release\btmanager.exe
```

## 📝 License

This project is for educational and development purposes.

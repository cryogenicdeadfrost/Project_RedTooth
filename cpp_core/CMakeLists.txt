cmake_minimum_required(VERSION 3.10)
project(BluetoothAudioCore)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_library(bt_core STATIC
    src/main.cpp
    src/DeviceScanner.cpp
    src/ConnectionPool.cpp
    src/ProfileManager.cpp
    src/Watchdog.cpp
    src/BluetoothManager.cpp
    src/AudioEngine.cpp
    src/AudioManager.cpp
)

target_link_libraries(bt_core PRIVATE 
    Bthprops.lib 
    Ws2_32.lib
    Mmdevapi.lib
    Ole32.lib
    Uuid.lib
)

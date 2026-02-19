#include <iostream>

extern "C" {
    void init_bluetooth_engine() {
        std::cout << "[C++] Bluetooth Engine Initialized" << std::endl;
    }
}

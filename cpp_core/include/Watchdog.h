#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include "ConnectionPool.h"

class Watchdog {
public:
    Watchdog(ConnectionPool* pool);
    ~Watchdog();

    void Start();
    void Stop();

private:
    void Loop();

    ConnectionPool* pool_;
    std::atomic<bool> running_;
    std::thread thread_;
};

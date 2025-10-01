#pragma once

#include <atomic>
#include <memory>
#include <thread>

class network {

    std::unique_ptr<std::thread> thread;
    std::atomic<bool> running;

public:
    network();
    ~network();

    static std::shared_ptr<network> &get();

    void startup();
    void shutdown();

};

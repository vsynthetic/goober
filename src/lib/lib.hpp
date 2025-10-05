#pragma once

#include <atomic>
#include <memory>

class lib {
    std::atomic<bool> initialized;

public:
    void init();
    void uninit();

    static std::shared_ptr<lib>& get();
};

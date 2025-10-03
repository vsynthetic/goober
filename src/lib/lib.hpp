#pragma once

#include "../java/java.hpp"
#include <atomic>
#include <memory>

class lib {
    std::atomic<bool> initialized;

public:
    java* jvm;

    void init();
    void uninit();

    static std::shared_ptr<lib>& get();
};

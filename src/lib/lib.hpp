#pragma once

#include "../java/java.hpp"
#include <memory>

class lib {
public:
    java* jvm;

    void init();
    void uninit();

    static std::shared_ptr<lib> &get();
};

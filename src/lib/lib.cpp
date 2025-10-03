#include "lib.hpp"
#include "../network/network.hpp"
#include <memory>

void lib::init() {
    if (initialized) return;

    jvm = java::get();

    network::get()->startup();

    initialized = true;
}

void lib::uninit() {
    if (!initialized) return;

    network::get()->shutdown();

    initialized = false;
}

std::shared_ptr<lib>& lib::get() {
    static std::shared_ptr<lib> g_lib = std::make_shared<lib>();
    return g_lib;
}

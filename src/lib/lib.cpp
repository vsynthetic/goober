#include "lib.hpp"
#include "../network/network.hpp"
#include <memory>

void lib::init() {
    jvm = java::get();

    network::get()->startup();
}

void lib::uninit() {
    network::get()->shutdown();
}

std::shared_ptr<lib> &lib::get() {
    static std::shared_ptr<lib> g_lib = std::make_shared<lib>();
    return g_lib;
}

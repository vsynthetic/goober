#include "lib.hpp"
#include "../network/network.hpp"
#include <filesystem>
#include <iostream>
#include <memory>

void lib::init() {
    jvm = java::get();

    std::cout << jvm->load_jar(std::filesystem::path("/home/lia/Code/java/generic/agent-test/build/libs/agent-test-1.0-SNAPSHOT.jar"), "cat.psychward.Agent") << std::endl;

    network::get()->startup();
}

void lib::uninit() {
    network::get()->shutdown();
}

std::shared_ptr<lib> &lib::get() {
    static std::shared_ptr<lib> g_lib = std::make_shared<lib>();
    return g_lib;
}

#include "network.hpp"
#include <memory>
#include <thread>
#include "../java/java.hpp"
#include "../ipc/ipc.hpp"

#ifdef _WIN32
#define PIPE_PATH "\\\\.\\pipe\\meow"
#else
#define PIPE_PATH "/tmp/meow.ipc"
#endif

std::shared_ptr<network> &network::get() {
    static std::shared_ptr<network> g_network = std::make_shared<network>();
    return g_network;
}

network::network() : thread(nullptr), running(false) {}

network::~network() {
    shutdown();

    if (thread != nullptr)
        thread.reset();
}

void network::startup() {
    running = true;

    thread = std::make_unique<std::thread>([this] {
        auto ipc = ipc_pipe(PIPE_PATH);
        auto jvm = java::get();

        while (running) {
            if (ipc.poll_client(200)) {
                // TODO: enter read/write loop here
                ipc.close_client();
            }
        }
    });
}

void network::shutdown() {
    running = false;

    if (thread != nullptr && thread->joinable())
        thread->join();
}

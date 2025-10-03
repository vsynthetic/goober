#include "network.hpp"
#include <cstring>
#include <filesystem>
#include <memory>
#include <thread>
#include "../java/java.hpp"
#include "../ipc/ipc.hpp"
#include "../lib/lib.hpp"
#include "messages.hpp"

#ifdef _WIN32
#define PIPE_PATH "\\\\.\\pipe\\meow"
#else
#define PIPE_PATH "/tmp/meow.ipc"
#endif

std::shared_ptr<network>& network::get() {
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
                message_type type;
                if (ipc.read_or_close(&type, sizeof(message_type)) == sizeof(message_type)) {
                    switch (type) {
                        case message_type::LOAD_JAR: {
                            auto size = sizeof(load_jar_message);
                            auto load = load_jar_message{};
                            memset(&load, 0, size);
                            if (ipc.read_or_close(&load, size) == size) {
                                // TODO: respond to pipe
                                jvm->load_jar(std::filesystem::path(std::string(load.path, 512)), std::string(load.entrypoint, 256));
                            }
                        } break;
                        case message_type::SHUTDOWN: {
                            lib::get()->uninit();
                            // TODO: this should also unload the library but that'll have to be done in the future!
                        } break;
                    }
                }
            }
        }
    });
}

void network::shutdown() {
    running = false;

    if (thread != nullptr && thread->joinable())
        thread->join();
}

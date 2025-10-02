#include "ipc.hpp"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/un.h>
#include <unistd.h>

static std::string get_message(int err) {
    char buf[256];
    strerror_r(err, buf, sizeof(buf));
    return std::string(buf, 256);
}

ipc_pipe::ipc_pipe(std::string _path) : fd(-1), client(-1) {
    path = std::filesystem::path(_path);
    auto str = path.string();

    if (std::filesystem::exists(path)) {
        std::cerr << "IPC socket path " << path << " exists pre-runtime. This shouldn't usually occur!" << std::endl;

        if (!std::filesystem::remove(path))
            std::cerr << "Failed to delete pre-existing IPC socket " << path << std::endl;
    }

    struct sockaddr_un saddr;
    if (str.size() >= sizeof(saddr.sun_path)) {
        std::cerr << "Socket path too long!" << std::endl;
        exit(1);
    }

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (fd == -1) {
        std::cerr << "Failed to create socket: " << get_message(errno) << std::endl;
        exit(1);
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, path.c_str(), sizeof(saddr.sun_path) - 1);

    if (bind(fd, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
        std::cerr << "Failed to bind socket to path " << path << ": "
            << get_message(errno) << std::endl;
        exit(1);
    }

    if (listen(fd, 1) == -1) {
        std::cerr << "Failed to listen on socket: " << get_message(errno) << std::endl;
        exit(1);
    }
}

bool ipc_pipe::poll_client(int timeout_ms) {
    if (client == -1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        struct timeval timeout = {
            .tv_sec = timeout_ms / 1000,
            .tv_usec = (timeout_ms % 1000) * 1000
        };

        int ready = select(fd + 1, &readfds, nullptr, nullptr, timeout_ms >= 0 ? &timeout : nullptr);

        if (ready > 0 && FD_ISSET(fd, &readfds)) {
            struct sockaddr_un addr;
            socklen_t addr_size;
            client = accept(fd, reinterpret_cast<sockaddr*>(&addr), reinterpret_cast<socklen_t*>(&addr_size));
            return client != -1;
        }
    }
    return false;
}

void ipc_pipe::close_client() {
    if (client != -1) {
        close(client);
        client = -1;
    }
}

bool ipc_pipe::is_connected() {
    return client != -1;
}

ipc_pipe::~ipc_pipe() {
    if (fd != -1) {
        close(fd);

        if (!std::filesystem::remove(path)) {
            std::cerr << "Failed to delete IPC socket under " << path << std::endl;
        }
    }

    if (client != -1) {
        close(client);
    }
}

size_t ipc_pipe::read(void *buffer, size_t size) {
    if (client == -1)
        return 0;

    return ::read(client, buffer, size);
}

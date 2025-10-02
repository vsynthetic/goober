#include <iostream>

#include "ipc.hpp"

ipc_pipe::ipc_pipe(std::string path) : pipe_handle(nullptr) {
    pipe_handle = CreateNamedPipeA(
        path.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        4096,
        4096,
        0,
        nullptr
    );

    if (pipe_handle == INVALID_HANDLE_VALUE) {
        char buf[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       buf, 256, nullptr);

        std::cerr << "CreateNamedPipeA failed: " << buf << std::endl;
    }
}

ipc_pipe::~ipc_pipe() {
    if (pipe_handle != nullptr) {
        CloseHandle(pipe_handle);
    }
}

bool ipc_pipe::poll_client(int timeout_ms) {
    return false;
}

void ipc_pipe::close_client() {
}

bool ipc_pipe::is_connected() {
    return false;
}

size_t ipc_pipe::read(void *buffer, size_t size) {
    DWORD bytes_read = 0;
    ReadFile(pipe_handle, buffer, size, &bytes_read, nullptr);
    return bytes_read;
}

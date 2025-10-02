#include <iostream>

#include "ipc.hpp"

ipc_pipe::ipc_pipe(std::string name) : name(name), pipe_handle(nullptr), connected(false) {
    pipe_handle = CreateNamedPipeA(
        name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        0,
        0,
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
        close_client();

        CloseHandle(pipe_handle);
    }
}

bool ipc_pipe::poll_client(int timeout_ms) {
    if (pipe_handle == INVALID_HANDLE_VALUE) {
        return connected = false;
    }

    connected = ConnectNamedPipe(pipe_handle, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    return connected;
}

void ipc_pipe::close_client() {
    if (pipe_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    FlushFileBuffers(pipe_handle);
    DisconnectNamedPipe(pipe_handle);
    connected = false;
}

bool ipc_pipe::is_connected() {
    return connected;
}

size_t ipc_pipe::read(void *buffer, size_t size) {
    if (!connected) {
        return 0;
    }

    DWORD bytes_read = 0;
    ReadFile(pipe_handle, buffer, size, &bytes_read, nullptr);
    return bytes_read;
}

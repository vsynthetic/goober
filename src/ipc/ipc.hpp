#pragma once

#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class ipc_pipe {

#ifdef _WIN32
    std::string name;
    HANDLE pipe_handle;
#else
    std::filesystem::path path;
    int fd;
    int client;
#endif

public:
    ipc_pipe(std::string path);
    ~ipc_pipe();

    bool poll_client(int timeout_ms = 0);
    void close_client();

    bool is_connected();

    size_t read(void *buffer, size_t size);

};

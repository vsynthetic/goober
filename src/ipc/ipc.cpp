#include "ipc.hpp"
#ifdef _WIN32
#include "ipc_win32.cpp"
#else
#include "ipc_linux.cpp"
#endif

size_t ipc_pipe::read_or_close(void* buffer, size_t size) {
    auto count = read(buffer, size);
    if (count <= 0)
        close_client();

    return count;
}

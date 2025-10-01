#include "lib/lib.hpp"

#ifndef __WIN32

__attribute__((constructor))
void load() {
    lib::get()->init();
}

__attribute__((destructor))
void unload() {
    lib::get()->uninit();
}

#else

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL WINAPI DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {

    if (reason == DLL_PROCESS_ATTACH) {
        lib::get()->init();
    } else if (reason == DLL_PROCESS_DETACH) {
        lib::get()->deinit();
    }

    return TRUE;
}

#endif

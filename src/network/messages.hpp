#pragma once

#include <cstdint>

enum class message_type : uint8_t {
    LOAD_JAR = 0,
    SHUTDOWN
};

struct load_jar_message {
    char path[512];
    char entrypoint[256];
};

#pragma once

// TODO: actually make this do something and write example application
struct load_jar_message {
    char path[512];
    char entrypoint[256];
};

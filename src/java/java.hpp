#pragma once

#include "jni.h"
#include "jvmti.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

enum class load_status : uint8_t {
    OK = 0,
    FILE_NOT_FOUND,
};

class java {

    std::unordered_map<std::string, jclass> class_map;
    bool dumped;

    JavaVM *m_jvm;
    JNIEnv *m_env;
    jvmtiEnv *m_ti;

    java();

    void dump();

public:

    static java* get();
    ~java();

    load_status load_jar(std::filesystem::path path, std::string agent_class);

    jclass get_class(std::string name);

};
